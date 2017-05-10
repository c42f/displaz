// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_POINTINPUT_INCLUDED
#define DISPLAZ_POINTINPUT_INCLUDED

#include <memory>

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStringList>

#include "Geometry.h"
#include "QtLogger.h"


/// Information passed along when a file is loaded
struct FileLoadInfo
{
    QString filePath;     /// Path to the file on disk
    QString dataSetLabel; /// Human readable label for the dataset
    bool replaceLabel;    /// Replace any existing dataset in the UI with the same label
    bool deleteAfterLoad; /// Delete file after load - for use with temporary files.
    bool mutateExisting;  /// Replace vertex data in-place and discard the result

    FileLoadInfo() : replaceLabel(true), deleteAfterLoad(false), mutateExisting(false) {}
    FileLoadInfo(const QString& filePath_, const QString& dataSetLabel_ = "",
                 bool replaceLabel_ = true)
        : filePath(filePath_),
        dataSetLabel(dataSetLabel_),
        replaceLabel(replaceLabel_),
        // Following must be set explicitly - getting it wrong will delete user data!
        deleteAfterLoad(false),
        mutateExisting(false)
    {
        if (dataSetLabel_.isEmpty())
        {
            QFileInfo fileInfo(filePath_);
            dataSetLabel = fileInfo.fileName();
        }
    }
};

Q_DECLARE_METATYPE(FileLoadInfo)


/// Loader for data files supported by displaz
///
/// This is run in a separate thread from the main GUI thread to maintain
/// responsiveness when loading large point clouds.
///
class FileLoader : public QObject
{
    Q_OBJECT
    public:
        FileLoader(size_t maxPointsPerFile, QObject* parent = 0)
            : QObject(parent),
            m_maxPointsPerFile(maxPointsPerFile)
        {
            qRegisterMetaType<FileLoadInfo>("FileLoadInfo");
        }

    public slots:
        /// Load file given by `loadInfo.filePath` asynchronously.  Threadsafe.
        ///
        /// -- NOTE WELL --
        /// If `loadInfo.deleteAfterLoad` is true, *delete* the file after loading.
        /// This is here so that temporary files typically loaded via a socket
        /// can be deleted in a clean way.
        void loadFile(const FileLoadInfo& loadInfo)
        {
            asyncLoadFile(loadInfo, false);
        }

        /// Reload file `loadInfo.filePath` asynchronously.  Threadsafe.
        void reloadFile(const FileLoadInfo& loadInfo)
        {
            asyncLoadFile(loadInfo, true);
        }

    signals:
        /// Signal emitted when a load step starts
        void loadStepStarted(QString description);

        /// Emitted to report progress percent for current load step
        void loadProgress(int percent);

        /// No more progress expected, task completed
        void resetProgress();

        /// Emitted on successfully loaded geometry
        ///
        /// If reloaded is true, the load signal is the result of calling
        /// reloadFile()
        void geometryLoaded(std::shared_ptr<Geometry> geom,
                            bool replaceLabel, bool reloaded);

        void geometryMutatorLoaded(std::shared_ptr<GeometryMutator> mutator);

    private slots:
        void asyncLoadFile(const FileLoadInfo& loadInfo, bool reloaded)
        {
            // Queued connection guff makes this async (+threadsafe)
            QMetaObject::invokeMethod(this, "loadFileImpl", Qt::QueuedConnection,
                                      Q_ARG(FileLoadInfo, loadInfo),
                                      Q_ARG(bool, reloaded));
        }

        void loadFileImpl(const FileLoadInfo& loadInfo, bool reloaded)
        {
            // Different codepath for mutating existing data.
            // TODO Geometry and GeometryMutator have different exception handling
            //      that could be made more consistent.
            if (loadInfo.mutateExisting)
            {
                std::shared_ptr<GeometryMutator> mutator(new GeometryMutator());
                if (!mutator->loadFile(loadInfo.filePath))
                {
                    g_logger.error("Could not load %s", loadInfo.filePath);
                    return;
                }
                mutator->moveToThread(0);
                mutator->setLabel(loadInfo.dataSetLabel);
                emit geometryMutatorLoaded(mutator);

                if (loadInfo.deleteAfterLoad)
                {
                    // Only delete after successful load:  Load errors
                    // are somewhat likely to be user error trying to load
                    // something they didn't mean to.
                    QFile::remove(loadInfo.filePath);
                }
                return;
            }

            // Standard loading code
            std::shared_ptr<Geometry> geom = Geometry::create(loadInfo.filePath);
            geom->setLabel(loadInfo.dataSetLabel);
            connect(geom.get(), SIGNAL(loadProgress(int)),
                    this, SIGNAL(loadProgress(int)));
            connect(geom.get(), SIGNAL(loadStepStarted(QString)),
                    this, SIGNAL(loadStepStarted(QString)));
            try
            {
                if (geom->loadFile(loadInfo.filePath, m_maxPointsPerFile))
                {
                    // Loader thread should disown the object so that its slots
                    // won't run until they're picked up by the main thread.
                    geom->moveToThread(0);
                    emit geometryLoaded(geom, loadInfo.replaceLabel, reloaded);
                    if (loadInfo.deleteAfterLoad)
                    {
                        // Only delete after successful load:  Load errors
                        // are somewhat likely to be user error trying to load
                        // something they didn't mean to.
                        QFile::remove(loadInfo.filePath);
                    }
                }
                else
                {
                    g_logger.error("Could not load %s", loadInfo.filePath);
                }
            }
            catch(std::bad_alloc& /*e*/)
            {
                g_logger.error("Ran out of memory trying to load %s", loadInfo.filePath);
            }
            catch(std::exception& e)
            {
                g_logger.error("Error loading %s: %s", loadInfo.filePath, e.what());
            }

            geom->disconnect();

            // Reset progress bar
            emit resetProgress();
        }

    private:
        size_t m_maxPointsPerFile;
};


#endif // DISPLAZ_POINTINPUT_INCLUDED
