/*
 *  See surfaceremeshgenerator.h.
 */
#include "surfaceremeshgenerator.h"
#include <QDebug>
#include <QElapsedTimer>

void SurfaceRemeshGenerator::process()
{
    QElapsedTimer timer;
    timer.start();

    std::vector<AutoRemesher::Vector3> vertices;
    std::vector<std::vector<size_t>> triangles;
    m_succeeded = AutoRemesher::ExternalRemesher::remesh(
        m_inVertices, m_inTriangles, m_parameters, vertices, triangles);

    if (m_succeeded) {
        m_outVertices = new std::vector<AutoRemesher::Vector3>(std::move(vertices));
        m_outTriangles = new std::vector<std::vector<size_t>>(std::move(triangles));
        qDebug() << "fTetWild surface remesh took" << timer.elapsed()
                 << "ms; vertices:" << m_outVertices->size()
                 << "triangles:" << m_outTriangles->size();
    } else {
        qDebug() << "fTetWild surface remesh failed after" << timer.elapsed() << "ms";
    }

    emit finished();
}
