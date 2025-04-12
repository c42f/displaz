// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt


#ifndef DISPLAZ_MAINWINDOW_H_INCLUDED
#define DISPLAZ_MAINWINDOW_H_INCLUDED

#include <QDir>
#include <QMainWindow>
#include <QSettings>

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
class HookManager;


//------------------------------------------------------------------------------
/// Main window for point cloud viewer application
class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(const QGLFormat& format);

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
        QByteArray hookPayload(QByteArray payload);

    protected:
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);
        void closeEvent(QCloseEvent *event) override;

    private slots:
        void fullScreen();
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
        void readSettings();
        void writeSettings();

    private:
        // Gui objects
        QProgressBar* m_progressBar;
        View3D* m_pointView;
        ShaderEditor* m_shaderEditor;
        HelpDialog* m_helpDialog;
        LogViewer* m_logTextView;

        // Gui state
        QString m_currShaderFileName;
        QSettings m_settings;

        // Actions
        QAction* m_open = nullptr;
        QAction* m_screenShot = nullptr;
        QAction* m_quit = nullptr;
        QAction* m_quitGeneric = nullptr;
        QAction* m_fullScreen = nullptr;
        QAction* m_trackBall = nullptr;

        // Dock Widgets
        QDockWidget* m_dockShaderEditor = nullptr;
        QDockWidget* m_dockShaderParameters = nullptr;
        QDockWidget* m_dockDataSet = nullptr;
        QDockWidget* m_dockLog = nullptr;

        // For full-screen toggle
        bool m_dockShaderEditorVisible = false;
        bool m_dockShaderParametersVisible = false;
        bool m_dockDataSetVisible = false;
        bool m_dockLogVisible = false;

        // File loader (slots run on separate thread)
        FileLoader* m_fileLoader;
        /// Maximum desired number of points to load
        size_t m_maxPointCount;
        // Currently loaded geometry
        GeometryCollection* m_geometries;

        // Interprocess communication
        QLocalServer* m_ipcServer;

        // Custom event registration for dynamic hooks
        HookManager* m_hookManager;
};


#endif // DISPLAZ_MAINWINDOW_H_INCLUDED
