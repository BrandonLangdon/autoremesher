/*
 *  Mesh importers for formats other than Wavefront OBJ.
 *
 *  Each loader fills a shared vertex list and a triangle-index list using the
 *  same representation MainWindow::loadObj produces, so the rest of the pipeline
 *  (island separation, isotropic remesh, quad-cover) is unchanged.
 */
#ifndef AUTO_REMESHER_MESH_IMPORTER_H
#define AUTO_REMESHER_MESH_IMPORTER_H
#include <AutoRemesher/Vector3>
#include <QString>
#include <vector>

namespace MeshImporter {

// STL (binary or ASCII). STL stores geometry per-triangle with no shared
// indices, so coincident vertices are welded to rebuild connectivity.
bool loadStl(const QString& filename,
    std::vector<AutoRemesher::Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles);

// 3MF (a ZIP container holding an XML mesh). Vertices are already indexed, so
// no welding is needed. Build-item transforms are not applied (objects are read
// in model space); connectivity is per-object so islands still separate.
bool load3mf(const QString& filename,
    std::vector<AutoRemesher::Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles);

}

#endif
