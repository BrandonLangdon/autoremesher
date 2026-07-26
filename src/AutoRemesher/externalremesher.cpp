/*
 *  See externalremesher.h.
 */
#include <AutoRemesher/ExternalRemesher>
#include <AutoRemesher/PositionKey>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace AutoRemesher {

namespace ExternalRemesher {

namespace {

const char* binaryPath()
{
    const char* p = std::getenv("AUTOREMESHER_FTETWILD");
    return (p && p[0]) ? p : nullptr;
}

// A unique-enough temp directory name without needing Date/random at namespace
// scope: a process-wide counter plus a wall-clock stamp.
std::filesystem::path makeWorkDir()
{
    static std::atomic<unsigned long long> counter { 0 };
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream name;
    name << "autoremesher_ftw_" << stamp << "_" << counter.fetch_add(1);
    std::filesystem::path dir = std::filesystem::temp_directory_path() / name.str();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

bool writeObj(const std::filesystem::path& path,
    const std::vector<Vector3>& vertices,
    const std::vector<std::vector<size_t>>& triangles)
{
    std::ofstream out(path);
    if (!out.is_open())
        return false;
    for (const auto& v : vertices)
        out << "v " << v.x() << ' ' << v.y() << ' ' << v.z() << '\n';
    for (const auto& tri : triangles) {
        if (tri.size() < 3)
            continue;
        out << "f " << (tri[0] + 1) << ' ' << (tri[1] + 1) << ' ' << (tri[2] + 1) << '\n';
    }
    return out.good();
}

// Parse the leading integer of an OBJ face token like "12", "12/3", "12/3/4".
long parseFaceIndex(const std::string& token)
{
    return std::strtol(token.c_str(), nullptr, 10);
}

bool readObjWelded(const std::filesystem::path& path,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles)
{
    std::ifstream in(path);
    if (!in.is_open())
        return false;
    std::vector<Vector3> rawVertices;
    std::vector<std::array<long, 3>> rawTriangles;
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 2)
            continue;
        if (line[0] == 'v' && line[1] == ' ') {
            std::istringstream ls(line.substr(2));
            double x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            rawVertices.push_back(Vector3(x, y, z));
        } else if (line[0] == 'f' && line[1] == ' ') {
            std::istringstream ls(line.substr(2));
            std::string a, b, c;
            ls >> a >> b >> c;
            if (a.empty() || b.empty() || c.empty())
                continue;
            rawTriangles.push_back({ parseFaceIndex(a), parseFaceIndex(b), parseFaceIndex(c) });
        }
    }
    if (rawVertices.empty() || rawTriangles.empty())
        return false;

    // Weld coincident vertices so the downstream connectivity-based pipeline sees
    // a shared-index mesh (fTetWild's surface is already clean; this is a
    // safety net and resolves any coincident output vertices).
    std::map<PositionKey, size_t> vertexMap;
    std::vector<size_t> remap(rawVertices.size());
    for (size_t i = 0; i < rawVertices.size(); ++i) {
        PositionKey key(rawVertices[i]);
        auto it = vertexMap.find(key);
        if (it != vertexMap.end()) {
            remap[i] = it->second;
        } else {
            remap[i] = outVertices.size();
            vertexMap.insert({ key, outVertices.size() });
            outVertices.push_back(rawVertices[i]);
        }
    }
    for (const auto& tri : rawTriangles) {
        // OBJ indices are 1-based; ignore malformed/degenerate faces.
        if (tri[0] < 1 || tri[1] < 1 || tri[2] < 1)
            continue;
        if ((size_t)tri[0] > rawVertices.size() || (size_t)tri[1] > rawVertices.size() || (size_t)tri[2] > rawVertices.size())
            continue;
        const size_t a = remap[tri[0] - 1], b = remap[tri[1] - 1], c = remap[tri[2] - 1];
        if (a == b || b == c || a == c)
            continue;
        outTriangles.push_back({ a, b, c });
    }
    return !outTriangles.empty();
}

double boundingBoxDiagonal(const std::vector<Vector3>& vertices)
{
    if (vertices.empty())
        return 0.0;
    Vector3 lo = vertices[0];
    Vector3 hi = vertices[0];
    for (const auto& v : vertices) {
        lo.setX(std::min(lo.x(), v.x()));
        lo.setY(std::min(lo.y(), v.y()));
        lo.setZ(std::min(lo.z(), v.z()));
        hi.setX(std::max(hi.x(), v.x()));
        hi.setY(std::max(hi.y(), v.y()));
        hi.setZ(std::max(hi.z(), v.z()));
    }
    const double dx = hi.x() - lo.x();
    const double dy = hi.y() - lo.y();
    const double dz = hi.z() - lo.z();
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::string shellQuote(const std::string& s)
{
    std::string out = "'";
    for (char c : s) {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    out += "'";
    return out;
}

} // namespace

bool isConfigured()
{
    const char* bin = binaryPath();
    if (nullptr == bin)
        return false;
    std::error_code ec;
    return std::filesystem::exists(bin, ec);
}

bool remesh(const std::vector<Vector3>& inVertices,
    const std::vector<std::vector<size_t>>& inTriangles,
    const Parameters& parameters,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles)
{
    const char* bin = binaryPath();
    if (nullptr == bin || inVertices.empty() || inTriangles.empty())
        return false;

    std::filesystem::path dir = makeWorkDir();
    if (dir.empty())
        return false;
    const std::filesystem::path inPath = dir / "in.obj";
    const std::filesystem::path outPath = dir / "out.msh";
    // fTetWild writes the extracted surface next to the tetmesh as "<out>__sf.obj".
    const std::filesystem::path surfacePath = dir / "out.msh__sf.obj";
    const std::filesystem::path logPath = dir / "log.txt";

    bool ok = false;
    if (writeObj(inPath, inVertices, inTriangles)) {
        std::ostringstream cmd;
        cmd << shellQuote(bin)
            << " -i " << shellQuote(inPath.string())
            << " -o " << shellQuote(outPath.string())
            << " --manifold-surface --is-quiet --level 4";
        // -a (absolute) and -l (relative) are mutually exclusive in fTetWild;
        // prefer absolute when the caller (the in-core per-island path) supplies it.
        if (parameters.edgeLengthAbs > 0.0) {
            double edgeLength = parameters.edgeLengthAbs;
            // Guard against a pathologically small edge length (e.g. a high target
            // quad count on a small/compact mesh drives the derived voxel size tiny):
            // fTetWild would build an enormous tetrahedralization and effectively
            // hang. Floor at 1% of the bounding-box diagonal, which keeps the run
            // bounded (~tens of seconds) while leaving normal requests untouched.
            const double diagonal = boundingBoxDiagonal(inVertices);
            const double minEdgeLength = diagonal * 0.01;
            if (diagonal > 0.0 && edgeLength < minEdgeLength) {
                std::cerr << "fTetWild: requested edge length " << edgeLength
                          << " is below 1% of the model size; clamping to " << minEdgeLength
                          << " to avoid an excessive tetrahedralization." << std::endl;
                edgeLength = minEdgeLength;
            }
            cmd << " -a " << edgeLength;
        } else if (parameters.edgeLengthRel > 0.0) {
            cmd << " -l " << parameters.edgeLengthRel;
        }
        if (parameters.envelopeSizeRel > 0.0)
            cmd << " -e " << parameters.envelopeSizeRel;
        if (parameters.coarsen)
            cmd << " --coarsen";
        cmd << " > " << shellQuote(logPath.string()) << " 2>&1";

        const int rc = std::system(cmd.str().c_str());
        std::error_code ec;
        if (rc == 0 && std::filesystem::exists(surfacePath, ec))
            ok = readObjWelded(surfacePath, outVertices, outTriangles);
    }

    // Keep the work dir when debugging is requested; otherwise clean up.
    if (nullptr == std::getenv("AUTOREMESHER_FTETWILD_KEEP")) {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    return ok;
}

}

}
