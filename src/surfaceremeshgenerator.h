/*
 *  Runs fTetWild on the whole loaded mesh as a standalone preprocessing step,
 *  off the UI thread. Unlike the in-core per-island path, this produces a single
 *  clean, watertight, uniform-resolution surface that becomes the working mesh
 *  fed to the quad remesher. Used by the GUI "Run fTetWild" button.
 */
#ifndef AUTO_REMESHER_SURFACE_REMESH_GENERATOR_H
#define AUTO_REMESHER_SURFACE_REMESH_GENERATOR_H
#include <AutoRemesher/ExternalRemesher>
#include <AutoRemesher/Vector3>
#include <QObject>
#include <vector>

class SurfaceRemeshGenerator : public QObject {
    Q_OBJECT
public:
    SurfaceRemeshGenerator(const std::vector<AutoRemesher::Vector3>& vertices,
        const std::vector<std::vector<size_t>>& triangles,
        const AutoRemesher::ExternalRemesher::Parameters& parameters)
        : m_inVertices(vertices)
        , m_inTriangles(triangles)
        , m_parameters(parameters)
    {
    }

    bool succeeded() const { return m_succeeded; }

    // Ownership of the results is transferred to the caller (nullptr on failure).
    std::vector<AutoRemesher::Vector3>* takeVertices()
    {
        std::vector<AutoRemesher::Vector3>* v = m_outVertices;
        m_outVertices = nullptr;
        return v;
    }

    std::vector<std::vector<size_t>>* takeTriangles()
    {
        std::vector<std::vector<size_t>>* t = m_outTriangles;
        m_outTriangles = nullptr;
        return t;
    }

    ~SurfaceRemeshGenerator()
    {
        delete m_outVertices;
        delete m_outTriangles;
    }

signals:
    void finished();

public slots:
    void process();

private:
    std::vector<AutoRemesher::Vector3> m_inVertices;
    std::vector<std::vector<size_t>> m_inTriangles;
    AutoRemesher::ExternalRemesher::Parameters m_parameters;
    std::vector<AutoRemesher::Vector3>* m_outVertices = nullptr;
    std::vector<std::vector<size_t>>* m_outTriangles = nullptr;
    bool m_succeeded = false;
};

#endif
