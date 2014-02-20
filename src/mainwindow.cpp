// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include "mainwindow.h"

#include "config.h"
#include "helpdialog.h"
#include "ptview.h"
#include "shadereditor.h"
#include "shader.h"
#include "mesh.h"
#include "logger.h"
#include "geometrycollection.h"
#include "datasetui.h"

#include <QtCore/QSignalMapper>
#include <QtGui/QApplication>
#include <QtGui/QColorDialog>
#include <QtGui/QDockWidget>
#include <QtGui/QFileDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QMenuBar>
#include <QtGui/QMessageBox>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QProgressBar>
#include <QtGui/QSplitter>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QDesktopWidget>
#include <QDropEvent>
#include <QUrl>


//------------------------------------------------------------------------------
// PointViewerMainWindow implementation

PointViewerMainWindow::PointViewerMainWindow()
    : m_progressBar(0),
    m_pointView(0),
    m_shaderEditor(0),
    m_helpDialog(0),
    m_logTextView(0),
    m_geometries(0)
{
    m_geometries = new GeometryCollection(this);
    connect(m_geometries, SIGNAL(layoutChanged()), this, SLOT(updateTitle()));
    connect(m_geometries, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateTitle()));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),    this, SLOT(updateTitle()));
    connect(m_geometries, SIGNAL(rowsRemoved(QModelIndex,int,int)),     this, SLOT(updateTitle()));

    setWindowTitle("Displaz");
    setAcceptDrops(true);
    m_helpDialog = new HelpDialog(this);

    //--------------------------------------------------
    // Menus
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAct = fileMenu->addAction(tr("&Open"));
    openAct->setToolTip(tr("Open a point cloud file"));
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, SIGNAL(triggered()), this, SLOT(openFiles()));
    QAction* reloadAct = fileMenu->addAction(tr("&Reload"));
    reloadAct->setStatusTip(tr("Reload point files from disk"));
    reloadAct->setShortcut(Qt::Key_F5);
    connect(reloadAct, SIGNAL(triggered()), this, SLOT(reloadFiles()));

    fileMenu->addSeparator();
    QAction* openShaderAct = fileMenu->addAction(tr("Open &Shader"));
    openAct->setToolTip(tr("Open a shader file"));
    connect(openShaderAct, SIGNAL(triggered()), this, SLOT(openShaderFile()));
    QAction* saveShaderAct = fileMenu->addAction(tr("Sa&ve Shader"));
    openAct->setToolTip(tr("Save current shader file"));
    connect(saveShaderAct, SIGNAL(triggered()), this, SLOT(saveShaderFile()));

    fileMenu->addSeparator();
    QAction* quitAct = fileMenu->addAction(tr("&Quit"));
    quitAct->setStatusTip(tr("Exit the application"));
    quitAct->setShortcuts(QKeySequence::Quit);
    connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QAction* trackballMode = viewMenu->addAction(tr("Use &Trackball camera"));
    trackballMode->setCheckable(true);
    trackballMode->setChecked(false);
    // Background sub-menu
    QMenu* backMenu = viewMenu->addMenu(tr("Set &Background"));
    QSignalMapper* mapper = new QSignalMapper(this);
    // Selectable backgrounds (svg_names from SVG standard - see QColor docs)
    const char* backgroundNames[] = {/* "Display Name", "svg_name", */
                                        "Default",      "default",
                                        "Black",        "black",
                                        "Dark Grey",    "dimgrey",
                                        "Slate Grey",   "slategrey",
                                        "Light Grey",   "lightgrey",
                                        "White",        "white" };
    for(size_t i = 0; i < sizeof(backgroundNames)/sizeof(const char*); i+=2)
    {
        QAction* backgroundAct = backMenu->addAction(tr(backgroundNames[i]));
        QPixmap pixmap(50,50);
        QString colName = backgroundNames[i+1];
        pixmap.fill(backgroundColFromName(colName));
        QIcon icon(pixmap);
        backgroundAct->setIcon(icon);
        mapper->setMapping(backgroundAct, colName);
        connect(backgroundAct, SIGNAL(triggered()), mapper, SLOT(map()));
    }
    connect(mapper, SIGNAL(mapped(QString)),
            this, SLOT(setBackground(QString)));
    backMenu->addSeparator();
    QAction* backgroundCustom = backMenu->addAction(tr("&Custom"));
    connect(backgroundCustom, SIGNAL(triggered()),
            this, SLOT(chooseBackground()));
    // Check boxes for drawing various scene elements by category
    viewMenu->addSeparator();
    QAction* drawBoundingBoxes = viewMenu->addAction(tr("Draw Bounding bo&xes"));
    drawBoundingBoxes->setCheckable(true);
    drawBoundingBoxes->setChecked(true);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* helpAct = helpMenu->addAction(tr("User &Guide"));
    connect(helpAct, SIGNAL(triggered()), this, SLOT(helpDialog()));
    helpMenu->addSeparator();
    QAction* aboutAct = helpMenu->addAction(tr("&About"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutDialog()));


    //--------------------------------------------------
    // Point viewer
    m_pointView = new PointView(m_geometries, this);
    setCentralWidget(m_pointView);
    connect(drawBoundingBoxes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawBoundingBoxes()));
    connect(trackballMode, SIGNAL(triggered()),
            m_pointView, SLOT(toggleCameraMode()));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(geometryRowsInserted(QModelIndex,int,int)));

    //--------------------------------------------------
    // Docked widgets
    // Shader parameters UI
    QDockWidget* shaderParamsDock = new QDockWidget(tr("Shader Parameters"), this);
    shaderParamsDock->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetClosable);
    shaderParamsDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                      Qt::RightDockWidgetArea);
    QWidget* shaderParamsUI = new QWidget(shaderParamsDock);
    shaderParamsDock->setWidget(shaderParamsUI);
    m_pointView->setShaderParamsUIWidget(shaderParamsUI);

    // Shader editor UI
    QDockWidget* shaderEditorDock = new QDockWidget(tr("Shader Editor"), this);
    shaderEditorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetClosable |
                                  QDockWidget::DockWidgetFloatable);
    shaderEditorDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                      Qt::RightDockWidgetArea);
    QWidget* shaderEditorUI = new QWidget(shaderEditorDock);
    m_shaderEditor = new ShaderEditor(shaderEditorUI);
    connect(m_shaderEditor, SIGNAL(sendShader(QString)),
            &m_pointView->shaderProgram(), SLOT(setShader(QString)));
    QGridLayout* shaderEditorLayout = new QGridLayout(shaderEditorUI);
    shaderEditorLayout->setContentsMargins(2,2,2,2);
    shaderEditorLayout->addWidget(m_shaderEditor, 0, 0, 1, 1);
    shaderEditorDock->setWidget(shaderEditorUI);

    // Log viewer UI
    QDockWidget* logDock = new QDockWidget(tr("Log"), this);
    logDock->setFeatures(QDockWidget::DockWidgetMovable |
                         QDockWidget::DockWidgetClosable);
    logDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    QWidget* logUI = new QWidget(logDock);
    m_logTextView = new LogViewer(logUI);
    m_logTextView->setReadOnly(true);
    m_logTextView->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_logTextView->connectLogger(&g_logger); // connect to global logger
    m_progressBar = new QProgressBar(logUI);
    m_progressBar->setRange(0,100);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    connect(m_geometries, SIGNAL(loadStepStarted(QString)),
            this, SLOT(setProgressBarText(QString)));
    connect(m_geometries, SIGNAL(loadProgress(int)),
            m_progressBar, SLOT(setValue(int)));
    connect(m_geometries, SIGNAL(fileLoadStarted()),
            m_progressBar, SLOT(show()));
    connect(m_geometries, SIGNAL(fileLoadFinished()),
            m_progressBar, SLOT(hide()));
    QVBoxLayout* logUILayout = new QVBoxLayout(logUI);
    //logUILayout->setContentsMargins(2,2,2,2);
    logUILayout->addWidget(m_logTextView);
    logUILayout->addWidget(m_progressBar);
    //m_logTextView->setLineWrapMode(QPlainTextEdit::NoWrap);
    logDock->setWidget(logUI);

    // Data set list UI
    QDockWidget* dataSetDock = new QDockWidget(tr("Data Sets"), this);
    dataSetDock->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetFloatable);
    dataSetDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    DataSetUI* dataSetUI = new DataSetUI(this);
    dataSetDock->setWidget(dataSetUI);
    QAbstractItemView* dataSetOverview = dataSetUI->view();
    dataSetOverview->setModel(m_geometries);
    connect(dataSetOverview, SIGNAL(doubleClicked(const QModelIndex&)),
            m_pointView, SLOT(centreOnGeometry(const QModelIndex&)));
    m_pointView->setSelectionModel(dataSetOverview->selectionModel());

    // Set up docked widgets
    addDockWidget(Qt::RightDockWidgetArea, shaderParamsDock);
    addDockWidget(Qt::LeftDockWidgetArea, shaderEditorDock);
    addDockWidget(Qt::RightDockWidgetArea, logDock);
    addDockWidget(Qt::RightDockWidgetArea, dataSetDock);
    tabifyDockWidget(logDock, dataSetDock);
    logDock->raise();
    shaderEditorDock->setVisible(false);

    // Add dock widget toggles to view menu
    viewMenu->addSeparator();
    viewMenu->addAction(shaderParamsDock->toggleViewAction());
    viewMenu->addAction(shaderEditorDock->toggleViewAction());
    viewMenu->addAction(logDock->toggleViewAction());
    viewMenu->addAction(dataSetDock->toggleViewAction());

    //--------------------------------------------------
    // Final setup
    openShaderFile("shaders:points_default.glsl");
}


void PointViewerMainWindow::setProgressBarText(QString text)
{
    m_progressBar->setFormat(text + " (%p%)");
}


void PointViewerMainWindow::geometryRowsInserted(const QModelIndex& parent, int first, int last)
{
    QItemSelection range(m_geometries->index(first), m_geometries->index(last));
    m_pointView->selectionModel()->select(range, QItemSelectionModel::Select);
}


void PointViewerMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        for (int i = 0; i < urls.size(); ++i)
        {
            if (urls[i].isLocalFile())
            {
                event->acceptProposedAction();
                break;
            }
        }
    }
}


void PointViewerMainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    QStringList droppedFiles;
    for (int i = 0; i < urls.size(); ++i)
    {
         if (urls[i].isLocalFile())
             droppedFiles << urls[i].toLocalFile();
    }
    m_geometries->loadFiles(droppedFiles);
}


QSize PointViewerMainWindow::sizeHint() const
{
    return QSize(800,600);
}


void PointViewerMainWindow::runCommand(const QByteArray& command)
{
    QList<QByteArray> commandTokens = command.split('\n');
    if (commandTokens.empty())
        return;
    if (commandTokens[0] == "OPEN_FILES")
    {
        QStringList files;
        for (int i = 1; i < commandTokens.size(); ++i)
            files << QString(commandTokens[i]);
        m_geometries->clear();
        m_geometries->loadFiles(files);
    }
    else if (commandTokens[0] == "SET_VIEW_ANGLES")
    {
        if (commandTokens.size()-1 != 3)
        {
            tfm::format(std::cerr, "Expected three view angles, got %d\n",
                        commandTokens.size()-1);
            return;
        }
        bool yawOk = false, pitchOk = false, rollOk = false;
        double yaw   = commandTokens[1].toDouble(&yawOk);
        double pitch = commandTokens[2].toDouble(&pitchOk);
        double roll  = commandTokens[3].toDouble(&rollOk);
        if (!yawOk || !pitchOk || !rollOk)
        {
            std::cerr << "Could not parse Euler angles for view\n";
            return;
        }
        m_pointView->camera().setRotation(
            QQuaternion::fromAxisAndAngle(0,0,1, roll)  *
            QQuaternion::fromAxisAndAngle(1,0,0, pitch-90) *
            QQuaternion::fromAxisAndAngle(0,0,1, yaw)
        );
    }
    else if (commandTokens[0] == "SET_VIEW_RADIUS")
    {
        bool ok = false;
        double viewRadius = commandTokens[1].toDouble(&ok);
        if (!ok)
        {
            std::cerr << "Could not parse view radius";
            return;
        }
        m_pointView->camera().setEyeToCenterDistance(viewRadius);
    }
    else
    {
        std::cerr << "Unkown socket command \"" << command << "\"\n";
    }
}

void PointViewerMainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape)
        close();
}


void PointViewerMainWindow::openFiles()
{
    // Note - using the static getOpenFileNames seems to be the only way to get
    // a native dialog.  This is annoying since the last selected directory
    // can't be deduced directly from the native dialog, but only when it
    // returns a valid selected file.  However we'll just have to put up with
    // this, because not having native dialogs is jarring.
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Open point clouds or meshes"),
        m_currFileDir.path(),
        tr("Point cloud files (*.las *.laz *.txt);;Mesh files (*.ply);;All files (*)"),
        0,
        QFileDialog::ReadOnly
    );
    if (files.empty())
        return;
    m_geometries->clear();
    m_geometries->loadFiles(files);
    m_currFileDir = QFileInfo(files[0]).dir();
    m_currFileDir.makeAbsolute();
}


void PointViewerMainWindow::openShaderFile(const QString& shaderFileName)
{
    QFile shaderFile(shaderFileName);
    if (shaderFile.open(QIODevice::ReadOnly))
    {
        m_currShaderFileName = shaderFile.fileName();
        QByteArray src = shaderFile.readAll();
        m_shaderEditor->setPlainText(src);
        m_pointView->shaderProgram().setShader(src);
    }
    else
    {
        g_logger.error("Couldn't open shader file \"%s\": %s",
                       shaderFileName, shaderFile.errorString());
    }
}


void PointViewerMainWindow::openShaderFile()
{
    QString shaderFileName = QFileDialog::getOpenFileName(
        this,
        tr("Open OpenGL shader in displaz format"),
        m_currShaderFileName,
        tr("OpenGL shader files (*.glsl);;All files(*)")
    );
    if (shaderFileName.isNull())
        return;
    openShaderFile(shaderFileName);
}


void PointViewerMainWindow::saveShaderFile()
{
    QString shaderFileName = QFileDialog::getSaveFileName(
        this,
        tr("Save current OpenGL shader"),
        m_currShaderFileName,
        tr("OpenGL shader files (*.glsl);;All files(*)")
    );
    if (shaderFileName.isNull())
        return;
    QFile shaderFile(shaderFileName);
    if (shaderFile.open(QIODevice::WriteOnly))
    {
        QTextStream stream(&shaderFile);
        stream << m_shaderEditor->toPlainText();
        m_currShaderFileName = shaderFileName;
    }
    else
    {
        g_logger.error("Couldn't open shader file \"%s\": %s",
                       shaderFileName, shaderFile.errorString());
    }
}


void PointViewerMainWindow::reloadFiles()
{
    m_geometries->reloadFiles();
}


void PointViewerMainWindow::helpDialog()
{
    m_helpDialog->show();
}


void PointViewerMainWindow::aboutDialog()
{
    QString message = tr(
        "<p><a href=\"http://c42f.github.io/displaz\"><b>Displaz</b></a> &mdash; a viewer for geospatial LiDAR data</p>"
        "<p>Version " DISPLAZ_VERSION_STRING "</p>"
        "<p>This software is open source under the BSD 3-clause license.  "
        "Source code is available at <a href=\"https://github.com/c42f/displaz\">https://github.com/c42f/displaz</a>.</p>"
    );
    QMessageBox::information(this, tr("About displaz"), message);
}


QColor PointViewerMainWindow::backgroundColFromName(const QString& name) const
{
    return (name == "default") ? QColor(60, 50, 50) : QColor(name);
}


void PointViewerMainWindow::setBackground(const QString& name)
{
    m_pointView->setBackground(backgroundColFromName(name));
}


void PointViewerMainWindow::chooseBackground()
{
    QColor originalColor = m_pointView->background();
    QColorDialog chooser(originalColor, this);
    connect(&chooser, SIGNAL(currentColorChanged(QColor)),
            m_pointView, SLOT(setBackground(QColor)));
    if (chooser.exec() == QDialog::Rejected)
        m_pointView->setBackground(originalColor);
}


void PointViewerMainWindow::updateTitle()
{
    QStringList fileNames;
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    for (auto i = geoms.begin(); i != geoms.end(); ++i)
        fileNames << QFileInfo((*i)->fileName()).fileName();
    setWindowTitle(tr("Displaz - %1").arg(fileNames.join(", ")));
}


