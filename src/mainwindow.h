// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt


#ifndef DISPLAZ_MAINWINDOW_H_INCLUDED
#define DISPLAZ_MAINWINDOW_H_INCLUDED

#include <QtCore/QDir>
#include <QtGui/QMainWindow>

#include <memory>

class QActionGroup;
class QLocalServer;
class QSignalMapper;
class QPlainTextEdit;
class QProgressBar;
class QModelIndex;
class QGLFormat;

class HelpDialog;
class View3D;
class ShaderEditor;
class LogViewer;
class GeometryCollection;
class IpcChannel;
class FileLoader;


//------------------------------------------------------------------------------
/// Main window for point cloud viewer application
class PointViewerMainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        PointViewerMainWindow(const QGLFormat& format);

        /// Hint at an appropriate size
        QSize sizeHint() const;

        /// Return file loader object
        FileLoader& fileLoader() { return *m_fileLoader; }

        /// Start server for interprocess communication
        ///
        /// Listens on local socket `socketName` for incoming connections.  Any
        /// socket previously in use is deleted.
        void startIpcServer(const QString& socketName);

    public slots:
        void handleMessage(QByteArray message);
        void openShaderFile(const QString& shaderFileName);

    protected:
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);

    private slots:
        void openFiles();
        void addFiles();
        void openShaderFile();
        void saveShaderFile();
        void compileShaderFile();
        void reloadFiles();
        void screenShot();
        void helpDialog();
        void aboutDialog();
        void setBackground(const QString& name);
        void chooseBackground();
        void updateTitle();
        void setProgressBarText(QString text);
        void geometryRowsInserted(const QModelIndex& parent, int first, int last);
        void handleIpcConnection();

    private:
        // Gui objects
        QProgressBar* m_progressBar;
        View3D* m_pointView;
        ShaderEditor* m_shaderEditor;
        HelpDialog* m_helpDialog;
        LogViewer* m_logTextView;

        // Gui state
        QDir m_currFileDir;
        QString m_currShaderFileName;

        // File loader (slots run on separate thread)
        FileLoader* m_fileLoader;
        /// Maximum desired number of points to load
        size_t m_maxPointCount;
        // Currently loaded geometry
        GeometryCollection* m_geometries;

        // Interprocess communication
        QLocalServer* m_ipcServer;
};


#endif // DISPLAZ_MAINWINDOW_H_INCLUDED
