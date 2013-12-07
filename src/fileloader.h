#ifndef DISPLAZ_POINTINPUT_INCLUDED
#define DISPLAZ_POINTINPUT_INCLUDED

#include <memory>

#include <QObject>
#include <QStringList>

#include "mesh.h"
#include "pointarray.h"

/// Loader for data files supported by displaz
///
/// This can be run in a separate thread from the main GUI thread to maintain
/// responsiveness when loading large point clouds.
///
class FileLoader : public QObject
{
    Q_OBJECT
    public:
        FileLoader(QStringList fileNames, size_t maxPointsPerFile,
                   QObject* parent = 0)
            : QObject(parent),
            m_fileNames(fileNames),
            m_maxPointsPerFile(maxPointsPerFile)
        { }

    public slots:
        /// Load all files provided to constructor
        void run()
        {
            loadFiles(m_fileNames, m_maxPointsPerFile);
        }

    signals:
        /// Signal emitted when a load step starts
        void loadStepStarted(QString description);
        /// Emitted to report progress percent for current load step
        void progress(int percent);

        /// Emit successfully loaded point file
        void pointsLoaded(std::shared_ptr<PointArray> lines);
        /// Emit successfully loaded mesh file
        void triMeshLoaded(std::shared_ptr<TriMesh> mesh);
        /// Emit successfully loaded line file
        void lineMeshLoaded(std::shared_ptr<LineSegments> lines);

        /// Emitted when loading of all files is done
        void finished();

    private:
        void loadFiles(const QStringList& fileNames, size_t maxPointsPerFile)
        {
            for(int i = 0; i < fileNames.size(); ++i)
            {
                const QString& fileName = fileNames[i];
                if(fileName.endsWith(".ply"))
                {
                    // Load data from ply format
                    std::shared_ptr<TriMesh> mesh;
                    std::shared_ptr<LineSegments> lineSegs;
                    if (readPlyFile(fileName, mesh, lineSegs))
                    {
                        if (mesh)
                            emit triMeshLoaded(mesh);
                        if (lineSegs)
                            emit lineMeshLoaded(lineSegs);
                    }
                }
                else
                {
                    // Load point cloud
                    std::shared_ptr<PointArray> points(new PointArray());
                    connect(points.get(), SIGNAL(pointsLoaded(int)),
                            this, SIGNAL(progress(int)));
                    connect(points.get(), SIGNAL(loadStepStarted(QString)),
                            this, SIGNAL(loadStepStarted(QString)));
                    if (points->loadPointFile(fileName, maxPointsPerFile) && !points->empty())
                        emit pointsLoaded(points);
                }
            }
            emit finished();
        }

        QStringList m_fileNames;
        size_t m_maxPointsPerFile;
};


#endif // DISPLAZ_POINTINPUT_INCLUDED
