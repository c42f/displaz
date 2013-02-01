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
#include "ptview.h"
#include "shadereditor.h"
#include "shader.h"

#include <QtCore/QSignalMapper>
#include <QtGui/QApplication>
#include <QtGui/QColorDialog>
#include <QtGui/QFileDialog>
#include <QtGui/QGridLayout>
#include <QtGui/QMenuBar>
#include <QtGui/QMessageBox>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QSplitter>
#include <QtGui/QDoubleSpinBox>
#include <QtGui/QDesktopWidget>

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

PointViewerMainWindow::PointViewerMainWindow(
        const QStringList& initialPointFileNames)
    : m_pointView(0),
    m_logTextView(0),
    m_shaderParamsTab(0),
    m_oldBuf(0)
{
    setWindowTitle("Displaz");

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
    QAction* quitAct = fileMenu->addAction(tr("&Quit"));
    quitAct->setStatusTip(tr("Exit the application"));
    quitAct->setShortcuts(QKeySequence::Quit);
    connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    QAction* drawBoundingBoxes = viewMenu->addAction(tr("Draw &Bounding boxes"));
    drawBoundingBoxes->setCheckable(true);
    drawBoundingBoxes->setChecked(true);
    QAction* trackballMode = viewMenu->addAction(tr("Use &Trackball camera"));
    trackballMode->setCheckable(true);
    trackballMode->setChecked(false);
    // Background sub-menu
    QMenu* backMenu = viewMenu->addMenu(tr("Set &Background"));
    QSignalMapper* mapper = new QSignalMapper(this);
    // Selectable backgrounds (svg_names from SVG standard - see QColor docs)
    const char* backgroundNames[] = {/* "Display Name", "svg_name", */
                                        "&Black",        "black",
                                        "&Dark Grey",    "dimgrey",
                                        "&Light Grey",   "lightgrey",
                                        "&White",        "white" };
    for(size_t i = 0; i < sizeof(backgroundNames)/sizeof(const char*); i+=2)
    {
        QAction* backgroundAct = backMenu->addAction(tr(backgroundNames[i]));
        mapper->setMapping(backgroundAct, backgroundNames[i+1]);
        connect(backgroundAct, SIGNAL(triggered()), mapper, SLOT(map()));
    }
    connect(mapper, SIGNAL(mapped(QString)),
            this, SLOT(setBackground(QString)));
    backMenu->addSeparator();
    QAction* backgroundCustom = backMenu->addAction(tr("&Custom"));
    connect(backgroundCustom, SIGNAL(triggered()),
            this, SLOT(chooseBackground()));

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* helpAct = helpMenu->addAction(tr("&Controls"));
    connect(helpAct, SIGNAL(triggered()), this, SLOT(helpDialog()));
    helpMenu->addSeparator();
    QAction* aboutAct = helpMenu->addAction(tr("&About"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutDialog()));

    // Central display area
    QSplitter* splitter = new QSplitter(this);
    setCentralWidget(splitter);

    // Point viewer
    m_pointView = new PointView(splitter);
    splitter->addWidget(m_pointView);
    splitter->setStretchFactor(0,10);
    connect(drawBoundingBoxes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawBoundingBoxes()));
    connect(trackballMode, SIGNAL(triggered()),
            m_pointView, SLOT(toggleCameraMode()));
    connect(m_pointView, SIGNAL(pointFilesLoaded(QStringList)),
            this, SLOT(setLoadedFileNames(QStringList)));

    // Tabbed widgets
    QTabWidget* tabs = new QTabWidget(splitter);
    splitter->addWidget(tabs);
    // Shader parameters tab
    m_shaderParamsTab = new QWidget(tabs);
    tabs->addTab(m_shaderParamsTab, tr("Shader Parameters"));
    connect(&m_pointView->shaderProgram(), SIGNAL(paramsChanged()),
            this, SLOT(setupShaderParamUI()));

    // Shader editor
    QWidget* shaderEditorTab = new QWidget(tabs);
    QGridLayout* shaderEditorTabLayout = new QGridLayout(shaderEditorTab);
    shaderEditorTabLayout->setContentsMargins(2,2,2,2);
    tabs->addTab(shaderEditorTab, tr("Shaders"));
    QTabWidget* shaderTabs = new QTabWidget(shaderEditorTab);
    shaderEditorTabLayout->addWidget(shaderTabs, 0, 0);
    // vertex shader
    ShaderEditor* vertexShaderEditor = new ShaderEditor(shaderTabs);
    shaderTabs->addTab(vertexShaderEditor, tr("Vertex Shader"));
    connect(vertexShaderEditor, SIGNAL(sendShader(QString)),
            &m_pointView->shaderProgram(), SLOT(setVertexShader(QString)));
    // fragment shader
    ShaderEditor* fragmentShaderEditor = new ShaderEditor(shaderTabs);
    shaderTabs->addTab(fragmentShaderEditor, tr("Fragment Shader"));
    connect(fragmentShaderEditor, SIGNAL(sendShader(QString)),
            &m_pointView->shaderProgram(), SLOT(setFragmentShader(QString)));

    // Log view tab
    m_logTextView = new QPlainTextEdit(tabs);
    m_logTextView->setReadOnly(true);
    m_logTextView->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_logTextView->setLineWrapMode(QPlainTextEdit::NoWrap);
    tabs->addTab(m_logTextView, tr("Log"));

    // Set shaders
    QFile vertexShaderFile(":/points_v.glsl");
    if (vertexShaderFile.open(QIODevice::ReadOnly))
        m_pointView->shaderProgram().setVertexShader(vertexShaderFile.readAll());
    QFile fragmentShaderFile(":/points_f.glsl");
    if (fragmentShaderFile.open(QIODevice::ReadOnly))
        m_pointView->shaderProgram().setFragmentShader(fragmentShaderFile.readAll());
    vertexShaderEditor->setPlainText(m_pointView->shaderProgram().vertexShader());
    fragmentShaderEditor->setPlainText(m_pointView->shaderProgram().fragmentShader());

    if(!initialPointFileNames.empty())
        m_pointView->loadPointFiles(initialPointFileNames);
}


PointViewerMainWindow::~PointViewerMainWindow()
{
    if (m_oldBuf)
        std::cout.rdbuf(m_oldBuf);
}


void PointViewerMainWindow::setupShaderParamUI()
{
    while (QWidget* child = m_shaderParamsTab->findChild<QWidget*>())
        delete child;
    delete m_shaderParamsTab->layout();
    m_pointView->shaderProgram().setupParameterUI(m_shaderParamsTab);
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


void PointViewerMainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape)
        close();
}


void PointViewerMainWindow::openFiles()
{
    QFileDialog dialog(this, tr("Select one or more point clouds to open"));
    dialog.setNameFilter(tr("Point cloud files (*.las *.laz)"));
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setDirectory(m_currFileDir);
    if(dialog.exec())
    {
        QStringList files = dialog.selectedFiles();
        m_pointView->loadPointFiles(files);
    }
    m_currFileDir = dialog.directory();
}


void PointViewerMainWindow::reloadFiles()
{
    m_pointView->reloadPointFiles();
}


void PointViewerMainWindow::helpDialog()
{
    QString message = tr(
        "<p><h2>Displaz 3D window controls</h2></p>"
        "<list>"
        "  <li>LMB+drag = rotate camera</li>"
        "  <li>RMB+drag = zoom camera</li>"
        "  <li>Ctrl+LMB+drag = move 3D cursor</li>"
        "  <li>Ctrl+RMB+drag = zoom 3D cursor along view direction</li>"
        "  <li>'c' = center camera on 3D cursor</li>"
        "  <li>'s' = snap 3D cursor to nearest point</li>"
        "</list>"
        "<p>(LMB, RMB = left & right mouse buttons)</p>"
    );
    QMessageBox::information(this, tr("Displaz control summary"), message);
}


void PointViewerMainWindow::aboutDialog()
{
    QString message = tr("Displaz - a qt-based las viewer\nversion 0.0.1");
    QMessageBox::information(this, tr("About displaz"), message);
}


void PointViewerMainWindow::setBackground(const QString& name)
{
    m_pointView->setBackground(QColor(name));
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

