/*
 *  Mesh importers for STL and 3MF. See meshimporter.h.
 */
#include "meshimporter.h"
#include <AutoRemesher/PositionKey>
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <cstring>
#include <map>
#include <zlib.h>

using AutoRemesher::PositionKey;
using AutoRemesher::Vector3;

namespace MeshImporter {

namespace {

uint32_t readU32LE(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint16_t readU16LE(const unsigned char* p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

float readF32LE(const unsigned char* p)
{
    float value;
    std::memcpy(&value, p, sizeof(float));
    return value;
}

// Append a welded vertex, returning its shared index.
size_t weldVertex(const Vector3& position,
    std::vector<Vector3>& outVertices,
    std::map<PositionKey, size_t>& vertexMap)
{
    PositionKey key(position);
    auto findResult = vertexMap.find(key);
    if (findResult != vertexMap.end())
        return findResult->second;
    size_t index = outVertices.size();
    outVertices.push_back(position);
    vertexMap.insert({ key, index });
    return index;
}

bool loadBinaryStl(const QByteArray& data,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles)
{
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    const uint32_t triangleCount = readU32LE(bytes + 80);
    // Header(80) + count(4) + 50 bytes/triangle.
    if ((size_t)data.size() < 84 + (size_t)triangleCount * 50)
        return false;
    std::map<PositionKey, size_t> vertexMap;
    outVertices.reserve(triangleCount * 3);
    outTriangles.reserve(triangleCount);
    for (uint32_t t = 0; t < triangleCount; ++t) {
        const unsigned char* tri = bytes + 84 + (size_t)t * 50;
        // Skip the 12-byte normal; read three vertices of three floats each.
        size_t triangle[3];
        for (int v = 0; v < 3; ++v) {
            const unsigned char* p = tri + 12 + v * 12;
            Vector3 position(readF32LE(p), readF32LE(p + 4), readF32LE(p + 8));
            triangle[v] = weldVertex(position, outVertices, vertexMap);
        }
        if (triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[0] == triangle[2])
            continue; // degenerate after welding
        outTriangles.push_back({ triangle[0], triangle[1], triangle[2] });
    }
    return !outTriangles.empty();
}

bool loadAsciiStl(const QByteArray& data,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles)
{
    std::map<PositionKey, size_t> vertexMap;
    // simplified() collapses all runs of whitespace to single spaces, so a plain
    // space split yields the tokens without pulling in QRegExp (removed in Qt6).
    const QString text = QString::fromUtf8(data).simplified();
    const QStringList tokens = text.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    std::vector<size_t> pending;
    for (int i = 0; i < tokens.size(); ++i) {
        if (tokens[i] != QLatin1String("vertex"))
            continue;
        if (i + 3 >= tokens.size())
            break;
        Vector3 position(tokens[i + 1].toDouble(), tokens[i + 2].toDouble(), tokens[i + 3].toDouble());
        pending.push_back(weldVertex(position, outVertices, vertexMap));
        if (pending.size() == 3) {
            if (!(pending[0] == pending[1] || pending[1] == pending[2] || pending[0] == pending[2]))
                outTriangles.push_back({ pending[0], pending[1], pending[2] });
            pending.clear();
        }
        i += 3;
    }
    return !outTriangles.empty();
}

// Inflate a single ZIP entry (stored or raw-deflate) into `out`.
bool inflateEntry(const unsigned char* src, size_t compressedSize, uint16_t method,
    size_t uncompressedSize, QByteArray& out)
{
    if (method == 0) { // stored
        out = QByteArray(reinterpret_cast<const char*>(src), (int)compressedSize);
        return true;
    }
    if (method != 8) // only deflate is used by 3MF
        return false;
    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
        return false;
    strm.next_in = const_cast<Bytef*>(src);
    strm.avail_in = (uInt)compressedSize;
    // uncompressedSize from the central directory is authoritative for 3MF; if
    // absent, grow the buffer as needed.
    size_t capacity = uncompressedSize > 0 ? uncompressedSize : compressedSize * 4 + 1024;
    out.resize((int)capacity);
    int ret = Z_OK;
    do {
        if (strm.total_out >= (uLong)capacity) {
            capacity *= 2;
            out.resize((int)capacity);
        }
        strm.next_out = reinterpret_cast<Bytef*>(out.data()) + strm.total_out;
        strm.avail_out = (uInt)(capacity - strm.total_out);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            return false;
        }
    } while (ret != Z_STREAM_END);
    out.resize((int)strm.total_out);
    inflateEnd(&strm);
    return true;
}

// Extract every 3D model part (an entry named "*.model") from a 3MF (ZIP)
// archive. Production 3MF (BambuStudio/Orca/Prusa) puts the root build in
// "3D/3dmodel.model" and the actual geometry in referenced "3D/Objects/*.model"
// parts, so all of them are inflated and returned for the caller to merge.
bool extractModelParts(const QByteArray& data, std::vector<QByteArray>& partsOut)
{
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    const size_t size = (size_t)data.size();
    if (size < 22)
        return false;
    // Find the End Of Central Directory record (may be followed by a comment).
    size_t eocd = 0;
    bool foundEocd = false;
    const size_t minPos = size >= (22 + 65535) ? size - (22 + 65535) : 0;
    for (size_t pos = size - 22 + 1; pos-- > minPos;) {
        if (readU32LE(bytes + pos) == 0x06054b50) {
            eocd = pos;
            foundEocd = true;
            break;
        }
    }
    if (!foundEocd)
        return false;
    const uint16_t totalEntries = readU16LE(bytes + eocd + 10);
    const uint32_t cdOffset = readU32LE(bytes + eocd + 16);
    if (cdOffset >= size)
        return false;
    // Walk the central directory, inflating every ".model" part.
    size_t cursor = cdOffset;
    for (uint16_t e = 0; e < totalEntries; ++e) {
        if (cursor + 46 > size || readU32LE(bytes + cursor) != 0x02014b50)
            break;
        const uint16_t method = readU16LE(bytes + cursor + 10);
        const uint32_t compressedSize = readU32LE(bytes + cursor + 20);
        const uint32_t uncompressedSize = readU32LE(bytes + cursor + 24);
        const uint16_t nameLen = readU16LE(bytes + cursor + 28);
        const uint16_t extraLen = readU16LE(bytes + cursor + 30);
        const uint16_t commentLen = readU16LE(bytes + cursor + 32);
        const uint32_t localOffset = readU32LE(bytes + cursor + 42);
        const QString name = QString::fromUtf8(
            reinterpret_cast<const char*>(bytes + cursor + 46), nameLen);
        cursor += 46 + nameLen + extraLen + commentLen;
        if (!name.endsWith(QLatin1String(".model"), Qt::CaseInsensitive))
            continue;
        // Read the local header to find where the entry data actually starts;
        // its name/extra lengths can differ from the central-directory copy.
        if (localOffset + 30 > size || readU32LE(bytes + localOffset) != 0x04034b50)
            continue;
        const uint16_t localNameLen = readU16LE(bytes + localOffset + 26);
        const uint16_t localExtraLen = readU16LE(bytes + localOffset + 28);
        const size_t dataStart = localOffset + 30 + localNameLen + localExtraLen;
        if (dataStart + compressedSize > size)
            continue;
        QByteArray part;
        if (inflateEntry(bytes + dataStart, compressedSize, method, uncompressedSize, part))
            partsOut.push_back(std::move(part));
    }
    return !partsOut.empty();
}

} // namespace

bool loadStl(const QString& filename,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();
    if (data.size() < 84)
        return false;
    // Binary STL: header(80) + uint32 count + 50 bytes/triangle exactly.
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    const uint32_t triangleCount = readU32LE(bytes + 80);
    const bool isBinary = (size_t)data.size() == 84 + (size_t)triangleCount * 50;
    const bool ok = isBinary
        ? loadBinaryStl(data, outVertices, outTriangles)
        : loadAsciiStl(data, outVertices, outTriangles);
    qDebug() << "loadStl:" << filename << (isBinary ? "(binary)" : "(ascii)")
             << "vertices:" << outVertices.size() << "triangles:" << outTriangles.size();
    return ok;
}

bool load3mf(const QString& filename,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();
    std::vector<QByteArray> parts;
    if (!extractModelParts(data, parts)) {
        qDebug() << "load3mf: could not extract any model part from" << filename;
        return false;
    }
    // Parse into raw (unwelded) arrays first. 3MF meshes routinely contain
    // coincident-but-distinct vertices; the connectivity-based pipeline (island
    // separation, quad_cover) needs those merged, so weld by position afterwards
    // the same way the STL loader does. Build-item transforms are not applied.
    std::vector<Vector3> rawVertices;
    std::vector<std::vector<size_t>> rawTriangles;
    for (const QByteArray& xml : parts) {
        QXmlStreamReader reader(xml);
        size_t meshVertexBase = rawVertices.size();
        while (!reader.atEnd()) {
            const QXmlStreamReader::TokenType token = reader.readNext();
            if (token != QXmlStreamReader::StartElement)
                continue;
            const QString name = reader.name().toString();
            if (name == QLatin1String("mesh")) {
                // A new mesh: subsequent vertex indices are local to it.
                meshVertexBase = rawVertices.size();
            } else if (name == QLatin1String("vertex")) {
                const QXmlStreamAttributes attrs = reader.attributes();
                rawVertices.push_back(Vector3(
                    attrs.value(QLatin1String("x")).toDouble(),
                    attrs.value(QLatin1String("y")).toDouble(),
                    attrs.value(QLatin1String("z")).toDouble()));
            } else if (name == QLatin1String("triangle")) {
                const QXmlStreamAttributes attrs = reader.attributes();
                const size_t v1 = meshVertexBase + attrs.value(QLatin1String("v1")).toULongLong();
                const size_t v2 = meshVertexBase + attrs.value(QLatin1String("v2")).toULongLong();
                const size_t v3 = meshVertexBase + attrs.value(QLatin1String("v3")).toULongLong();
                if (v1 < rawVertices.size() && v2 < rawVertices.size() && v3 < rawVertices.size())
                    rawTriangles.push_back({ v1, v2, v3 });
            }
        }
        // One malformed part should not abort the whole load; skip it and keep
        // whatever the other parts contributed.
        if (reader.hasError())
            qDebug() << "load3mf: XML parse error in a part:" << reader.errorString();
    }

    // Weld coincident vertices and remap the triangles onto the shared indices.
    std::map<PositionKey, size_t> vertexMap;
    std::vector<size_t> remap(rawVertices.size());
    for (size_t i = 0; i < rawVertices.size(); ++i)
        remap[i] = weldVertex(rawVertices[i], outVertices, vertexMap);
    for (const auto& tri : rawTriangles) {
        const size_t a = remap[tri[0]], b = remap[tri[1]], c = remap[tri[2]];
        if (a == b || b == c || a == c)
            continue; // degenerate after welding
        outTriangles.push_back({ a, b, c });
    }

    qDebug() << "load3mf:" << filename
             << "vertices:" << outVertices.size() << "triangles:" << outTriangles.size();
    return !outTriangles.empty();
}

}
