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
#include <QUrl>
#include <QDropEvent>

//------------------------------------------------------------------------------
// Unbuffered std::streambuf implementation for output to a text edit widget
class StreamBufTextEditSink : public std::streambuf
{
    public:
        StreamBufTextEditSink(QPlainTextEdit* textEdit) : m_textEdit(textEdit) {}

    protected:
        int overflow(int c)
        {
            m_textEdit->moveCursor(QTextCursor::End);
            m_textEdit->insertPlainText(QString((char)c));
            m_textEdit->ensureCursorVisible();
            return 0;
        }

    private:
        QPlainTextEdit* m_textEdit;
};


//------------------------------------------------------------------------------
// PointViewerMainWindow implementation

PointViewerMainWindow::PointViewerMainWindow()
    : m_progressBar(0),
    m_pointView(0),
    m_shaderEditor(0),
    m_helpDialog(0),
    m_logTextView(0),
    m_oldBuf(0)
{
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
    QAction* simplifyAction = viewMenu->addAction(tr("&Simplify point cloud"));
    simplifyAction->setCheckable(true);
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
    QAction* drawPoints = viewMenu->addAction(tr("Draw &Points"));
    drawPoints->setCheckable(true);
    drawPoints->setChecked(true);
    QAction* drawMeshes = viewMenu->addAction(tr("Draw &Meshes"));
    drawMeshes->setCheckable(true);
    drawMeshes->setChecked(true);

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* helpAct = helpMenu->addAction(tr("User &Guide"));
    connect(helpAct, SIGNAL(triggered()), this, SLOT(helpDialog()));
    helpMenu->addSeparator();
    QAction* aboutAct = helpMenu->addAction(tr("&About"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutDialog()));


    //--------------------------------------------------
    // Point viewer
    m_pointView = new PointView(this);
    setCentralWidget(m_pointView);
    connect(drawBoundingBoxes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawBoundingBoxes()));
    connect(drawPoints, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawPoints()));
    connect(drawMeshes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawMeshes()));
    connect(trackballMode, SIGNAL(triggered()),
            m_pointView, SLOT(toggleCameraMode()));
    connect(simplifyAction, SIGNAL(toggled(bool)),
            m_pointView, SLOT(setStochasticSimplification(bool)));
    connect(m_pointView, SIGNAL(pointFilesLoaded(QStringList)),
            this, SLOT(setLoadedFileNames(QStringList)));
    simplifyAction->setChecked(true);

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
    m_logTextView = new QPlainTextEdit(logUI);
    m_logTextView->setReadOnly(true);
    m_logTextView->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_progressBar = new QProgressBar(logUI);
    m_progressBar->setRange(0,100);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    connect(m_pointView, SIGNAL(loadStepStarted(QString)),
            this, SLOT(setProgressBarText(QString)));
    connect(m_pointView, SIGNAL(pointsLoaded(int)),
            m_progressBar, SLOT(setValue(int)));
    connect(m_pointView, SIGNAL(fileLoadStarted()),
            m_progressBar, SLOT(show()));
    connect(m_pointView, SIGNAL(fileLoadFinished()),
            m_progressBar, SLOT(hide()));
    QVBoxLayout* logUILayout = new QVBoxLayout(logUI);
    logUILayout->setContentsMargins(2,2,2,2);
    logUILayout->addWidget(m_logTextView);
    logUILayout->addWidget(m_progressBar);
    //m_logTextView->setLineWrapMode(QPlainTextEdit::NoWrap);
    logDock->setWidget(logUI);

    // Set up docked widgets
    addDockWidget(Qt::RightDockWidgetArea, shaderParamsDock);
    addDockWidget(Qt::LeftDockWidgetArea, shaderEditorDock);
    addDockWidget(Qt::RightDockWidgetArea, logDock);
    //tabifyDockWidget(shaderParamsDock, shaderEditorDock);
    shaderEditorDock->setVisible(false);

    // Add dock widget toggles to view menu
    viewMenu->addSeparator();
    viewMenu->addAction(shaderParamsDock->toggleViewAction());
    viewMenu->addAction(shaderEditorDock->toggleViewAction());
    viewMenu->addAction(logDock->toggleViewAction());

    //--------------------------------------------------
    // Final setup
    openShaderFile("shaders:points_default.glsl");
}


PointViewerMainWindow::~PointViewerMainWindow()
{
    if (m_oldBuf)
        std::cout.rdbuf(m_oldBuf);
}


void PointViewerMainWindow::setProgressBarText(QString text)
{
    m_progressBar->setFormat(text + " (%p%)");
}

void PointViewerMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
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
    m_pointView->loadFiles(droppedFiles);
}


QSize PointViewerMainWindow::sizeHint() const
{
    return QSize(800,600);
}


void PointViewerMainWindow::captureStdout()
{
    m_oldBuf = std::cout.rdbuf();
    m_guiStdoutBuf.reset(new StreamBufTextEditSink(m_logTextView));
    std::cout.rdbuf(m_guiStdoutBuf.get());
}

void PointViewerMainWindow::runCommand(const QByteArray& command)
{
    QList<QByteArray> commandTokens = command.split('\n');
    if (commandTokens.empty())
        return;
    if (commandTokens[0] != "OPEN_FILES")
    {
        std::cout << "Unkown command " << command.data() << "\n";
        return;
    }
    QStringList files;
    for (int i = 1; i < commandTokens.size(); ++i)
        files << QString(commandTokens[i]);
    m_pointView->loadFiles(files);
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
    m_pointView->loadFiles(files);
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
        QMessageBox::critical(0, tr("Error"),
            tr("Couldn't open shader file \"%1\": %2")
                .arg(shaderFileName)
                .arg(shaderFile.errorString())
        );
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
        QMessageBox::critical(0, tr("Error"),
            tr("Couldn't open shader file \"%1\": %2")
                .arg(shaderFileName)
                .arg(shaderFile.errorString())
        );
    }
}


void PointViewerMainWindow::reloadFiles()
{
    m_pointView->reloadFiles();
}


void PointViewerMainWindow::helpDialog()
{
    m_helpDialog->show();
}


void PointViewerMainWindow::aboutDialog()
{
    QString message = tr(
        "<p><b>Displaz</b> - a viewer for geospatial LiDAR data</p>"
        "<p>version " DISPLAZ_VERSION_STRING "</p>"
        "<br/>"
        "<p>This software is released under the BSD 3-clause license.  "
        "Code is available at <a href=\"https://github.com/c42f/displaz\">https://github.com/c42f/displaz</a>.</p>"
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
    m_pointView->setBackground(
        QColorDialog::getColor(QColor(255,255,255), this, "background color"));
}


void PointViewerMainWindow::setLoadedFileNames(const QStringList& fileNames)
{
    setWindowTitle(tr("Displaz - %1").arg(fileNames.join(", ")));
}


void PointViewerMainWindow::openInitialFiles()
{
    if (!g_initialFileNames.empty())
        m_pointView->loadFiles(g_initialFileNames);
}

