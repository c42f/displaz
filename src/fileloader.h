// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_POINTINPUT_INCLUDED
#define DISPLAZ_POINTINPUT_INCLUDED

#include <memory>

#include <QFile>
#include <QObject>
#include <QStringList>

#include "geometry.h"
#include "qtlogger.h"


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
        { }

    public slots:
        /// Load file given by `fileName` asynchronously.  Threadsafe.
        ///
        /// -- NOTE WELL --
        /// If `deleteAfterLoad` is true, *delete* the file after loading.
        /// This is here so that temporary files typically loaded via a socket
        /// can be deleted in a clean way.
        void loadFile(QString fileName, bool deleteAfterLoad = false)
        {
            asyncLoadFile(fileName, deleteAfterLoad, false);
        }

        /// Reload file `fileName` asynchronously.  Threadsafe.
        void reloadFile(QString fileName)
        {
            asyncLoadFile(fileName, false, true);
        }

    signals:
        /// Signal emitted when a load step starts
        void loadStepStarted(QString description);

        /// Emitted to report progress percent for current load step
        void loadProgress(int percent);

        /// Emitted on successfully loaded geometry
        ///
        /// If reloaded is true, the load signal is the result of calling
        /// reloadFile()
        void geometryLoaded(std::shared_ptr<Geometry> geom,
                            bool reloaded);

    private slots:
        void asyncLoadFile(QString fileName, bool deleteAfterLoad,
                           bool reloaded)
        {
            // Queued connection guff makes this async (+threadsafe)
            QMetaObject::invokeMethod(this, "loadFileImpl", Qt::QueuedConnection,
                                      Q_ARG(QString, fileName),
                                      Q_ARG(bool, deleteAfterLoad),
                                      Q_ARG(bool, reloaded));
        }

        void loadFileImpl(QString fileName, bool deleteAfterLoad,
                          bool reloaded)
        {
            std::shared_ptr<Geometry> geom = Geometry::create(fileName);
            connect(geom.get(), SIGNAL(loadProgress(int)),
                    this, SIGNAL(loadProgress(int)));
            connect(geom.get(), SIGNAL(loadStepStarted(QString)),
                    this, SIGNAL(loadStepStarted(QString)));
            try
            {
                if (geom->loadFile(fileName, m_maxPointsPerFile))
                {
                    emit geometryLoaded(geom, reloaded);
                    if (deleteAfterLoad)
                    {
                        // Only delete after successful load:  Load errors
                        // are somewhat likely to be user error trying to load
                        // something they didn't mean to.
                        QFile::remove(fileName);
                    }
                }
                else
                {
                    g_logger.error("Could not load %s", fileName);
                }
            }
            catch(std::bad_alloc& /*e*/)
            {
                g_logger.error("Ran out of memory trying to load %s", fileName);
            }
            catch(std::exception& e)
            {
                g_logger.error("Error loading %s: %s", fileName, e.what());
            }
        }

    private:
        size_t m_maxPointsPerFile;
};


#endif // DISPLAZ_POINTINPUT_INCLUDED
