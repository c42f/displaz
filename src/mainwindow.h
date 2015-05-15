// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt


#ifndef DISPLAZ_MAINWINDOW_H_INCLUDED
#define DISPLAZ_MAINWINDOW_H_INCLUDED

#include <QtCore/QDir>
#include <QtGui/QMainWindow>

#include <memory>

class QActionGroup;
class QLocalSocket;
class QSignalMapper;
class QPlainTextEdit;
class QProgressBar;
class QModelIndex;

class HelpDialog;
class View3D;
class ShaderEditor;
class LogViewer;
class GeometryCollection;


//------------------------------------------------------------------------------
/// Main window for point cloud viewer application
class PointViewerMainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        PointViewerMainWindow();

        /// Hint at an appropriate size
        QSize sizeHint() const;

        GeometryCollection& geometries() { return *m_geometries; }

    public slots:
        void runCommand(const QByteArray& command, QLocalSocket* connection);
        void openShaderFile(const QString& shaderFileName);

    protected:
        void keyReleaseEvent(QKeyEvent* event);
        void dragEnterEvent(QDragEnterEvent *event);
        void dropEvent(QDropEvent *event);

    private slots:
        void openFiles();
        void addFiles();
        void openShaderFile();
        void saveShaderFile();
        void reloadFiles();
        void screenShot();
        void helpDialog();
        void aboutDialog();
        void setBackground(const QString& name);
        void chooseBackground();
        void updateTitle();
        void setProgressBarText(QString text);
        void geometryRowsInserted(const QModelIndex& parent, int first, int last);

    private:
        QColor backgroundColFromName(const QString& name) const;

        // Gui objects
        QProgressBar* m_progressBar;
        View3D* m_pointView;
        ShaderEditor* m_shaderEditor;
        HelpDialog* m_helpDialog;
        LogViewer* m_logTextView;

        // Gui state
        QDir m_currFileDir;
        QString m_currShaderFileName;

        // Currently loaded geometry
        GeometryCollection* m_geometries;
};


#endif // DISPLAZ_MAINWINDOW_H_INCLUDED
