/*
 *  External remesher bridge: hands a triangle mesh to fTetWild (FloatTetwild_bin)
 *  as a subprocess and reads back the clean, watertight, uniform-resolution
 *  surface it extracts. Used as a robust, scalable alternative to the built-in
 *  isotropic remesher for large/dense inputs that the latter cannot handle in
 *  reasonable time.
 *
 *  The mesher binary is located via the AUTOREMESHER_FTETWILD environment
 *  variable (absolute path to FloatTetwild_bin). If unset or missing, the bridge
 *  reports itself unavailable and the caller falls back to the built-in remesher.
 */
#ifndef AUTO_REMESHER_EXTERNAL_REMESHER_H
#define AUTO_REMESHER_EXTERNAL_REMESHER_H
#include <AutoRemesher/Vector3>
#include <vector>

namespace AutoRemesher {

namespace ExternalRemesher {

// True when a usable mesher binary is configured (env var set and file exists).
bool isConfigured();

// Remesh one triangle mesh via fTetWild. targetEdgeLength is the absolute ideal
// edge length (same units as the input, i.e. AutoRemesher's voxel size).
// Returns true and fills outVertices/outTriangles (a welded triangle surface) on
// success; false on any failure so the caller can fall back.
bool remesh(const std::vector<Vector3>& inVertices,
    const std::vector<std::vector<size_t>>& inTriangles,
    double targetEdgeLength,
    std::vector<Vector3>& outVertices,
    std::vector<std::vector<size_t>>& outTriangles);

}

}

#endif
