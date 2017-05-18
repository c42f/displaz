// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "PointViewerMainWindow.h"

#include "config.h"
#include "DataSetUI.h"
#include "fileloader.h"
#include "geometrycollection.h"
#include "HelpDialog.h"
#include "IpcChannel.h"
#include "QtLogger.h"
#include "TriMesh.h"
#include "ShaderEditor.h"
#include "Shader.h"
#include "View3D.h"
#include "HookFormatter.h"
#include "HookManager.h"

#include <QSignalMapper>
#include <QThread>
#include <QUrl>
#include <QApplication>
#include <QColorDialog>
#include <QDockWidget>
#include <QFileDialog>
#include <QGridLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSplitter>
#include <QDoubleSpinBox>
#include <QDesktopWidget>
#include <QDropEvent>
#include <QLocalServer>
#include <QMimeData>
#include <QGLFormat>

//------------------------------------------------------------------------------
// PointViewerMainWindow implementation

PointViewerMainWindow::PointViewerMainWindow(const QGLFormat& format)
    : m_progressBar(0),
    m_pointView(0),
    m_shaderEditor(0),
    m_helpDialog(0),
    m_logTextView(0),
    m_maxPointCount(200*1000*1000), // 200 million
    m_geometries(0),
    m_ipcServer(0),
    m_hookManager(0)
{
#ifndef _WIN32
    setWindowIcon(QIcon(":resource/displaz_icon_256.png"));
#else
    // On windows, application icon is set via windows resource file
#endif
    setWindowTitle("Displaz");
    setAcceptDrops(true);

    m_helpDialog = new HelpDialog(this);

    m_geometries = new GeometryCollection(this);
    connect(m_geometries, SIGNAL(layoutChanged()), this, SLOT(updateTitle()));
    connect(m_geometries, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(updateTitle()));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),    this, SLOT(updateTitle()));
    connect(m_geometries, SIGNAL(rowsRemoved(QModelIndex,int,int)),     this, SLOT(updateTitle()));

    //--------------------------------------------------
    // Set up file loader in a separate thread
    //
    // Some subtleties regarding qt thread usage are discussed here:
    // http://mayaposch.wordpress.com/2011/11/01/how-to-really-truly-use-qthreads-the-full-explanation
    //
    // Main point: each QObject has a thread affinity which determines which
    // thread its slots will execute on, when called via a connected signal.
    QThread* loaderThread = new QThread();
    m_fileLoader = new FileLoader(m_maxPointCount);
    m_fileLoader->moveToThread(loaderThread);
    connect(loaderThread, SIGNAL(finished()), m_fileLoader, SLOT(deleteLater()));
    connect(loaderThread, SIGNAL(finished()), loaderThread, SLOT(deleteLater()));
    //connect(m_fileLoader, SIGNAL(finished()), this, SIGNAL(fileLoadFinished()));
    connect(m_fileLoader, SIGNAL(geometryLoaded(std::shared_ptr<Geometry>, bool, bool)),
            m_geometries, SLOT(addGeometry(std::shared_ptr<Geometry>, bool, bool)));
    connect(m_fileLoader, SIGNAL(geometryMutatorLoaded(std::shared_ptr<GeometryMutator>)),
            m_geometries, SLOT(mutateGeometry(std::shared_ptr<GeometryMutator>)));
    loaderThread->start();

    //--------------------------------------------------
    // Menus
    menuBar()->setNativeMenuBar(false); // OS X doesn't activate the native menu bar under Qt5

    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAct = fileMenu->addAction(tr("&Open"));
    openAct->setToolTip(tr("Open a data set"));
    openAct->setShortcuts(QKeySequence::Open);
    connect(openAct, SIGNAL(triggered()), this, SLOT(openFiles()));
    QAction* addAct = fileMenu->addAction(tr("&Add"));
    addAct->setToolTip(tr("Add a data set"));
    connect(addAct, SIGNAL(triggered()), this, SLOT(addFiles()));
    QAction* reloadAct = fileMenu->addAction(tr("&Reload"));
    reloadAct->setStatusTip(tr("Reload point files from disk"));
    reloadAct->setShortcut(Qt::Key_F5);
    connect(reloadAct, SIGNAL(triggered()), this, SLOT(reloadFiles()));

    fileMenu->addSeparator();
    QAction* screenShotAct = fileMenu->addAction(tr("Scree&nshot"));
    screenShotAct->setStatusTip(tr("Save screen shot of 3D window"));
    screenShotAct->setShortcut(Qt::Key_F9);
    connect(screenShotAct, SIGNAL(triggered()), this, SLOT(screenShot()));

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
                                        "Default",      "#3C3232",
                                        "Black",        "black",
                                        "Dark Grey",    "dimgrey",
                                        "Slate Grey",   "#858C93",
                                        "Light Grey",   "lightgrey",
                                        "White",        "white" };
    for(size_t i = 0; i < sizeof(backgroundNames)/sizeof(const char*); i+=2)
    {
        QAction* backgroundAct = backMenu->addAction(tr(backgroundNames[i]));
        QPixmap pixmap(50,50);
        QString colName = backgroundNames[i+1];
        pixmap.fill(QColor(colName));
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
    QAction* drawCursor = viewMenu->addAction(tr("Draw 3D &Cursor"));
    drawCursor->setCheckable(true);
    drawCursor->setChecked(true);
    QAction* drawAxes = viewMenu->addAction(tr("Draw &Axes"));
    drawAxes->setCheckable(true);
    drawAxes->setChecked(true);
    QAction* drawGrid = viewMenu->addAction(tr("Draw &Grid"));
    drawGrid->setCheckable(true);
    drawGrid->setChecked(false);
    QAction* drawAnnotations = viewMenu->addAction(tr("Draw A&nnotations"));
    drawAnnotations->setCheckable(true);
    drawAnnotations->setChecked(true);

    // Shader menu
    QMenu* shaderMenu = menuBar()->addMenu(tr("&Shader"));
    QAction* openShaderAct = shaderMenu->addAction(tr("&Open"));
    openShaderAct->setToolTip(tr("Open a shader file"));
    connect(openShaderAct, SIGNAL(triggered()), this, SLOT(openShaderFile()));
    QAction* editShaderAct = shaderMenu->addAction(tr("&Edit"));
    editShaderAct->setToolTip(tr("Open shader editor window"));
    QAction* saveShaderAct = shaderMenu->addAction(tr("&Save"));
    saveShaderAct->setToolTip(tr("Save current shader file"));
    connect(saveShaderAct, SIGNAL(triggered()), this, SLOT(saveShaderFile()));
    shaderMenu->addSeparator();

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* helpAct = helpMenu->addAction(tr("User &Guide"));
    connect(helpAct, SIGNAL(triggered()), this, SLOT(helpDialog()));
    helpMenu->addSeparator();
    QAction* aboutAct = helpMenu->addAction(tr("&About"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(aboutDialog()));


    //--------------------------------------------------
    // Point viewer
    m_pointView = new View3D(m_geometries, format, this);
    setCentralWidget(m_pointView);
    connect(drawBoundingBoxes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawBoundingBoxes()));
    connect(drawCursor, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawCursor()));
    connect(drawAxes, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawAxes()));
    connect(drawGrid, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawGrid()));
    connect(drawAnnotations, SIGNAL(triggered()),
            m_pointView, SLOT(toggleDrawAnnotations()));
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
    QWidget* shaderParamsUI = new QWidget(shaderParamsDock);
    shaderParamsDock->setWidget(shaderParamsUI);
    m_pointView->setShaderParamsUIWidget(shaderParamsUI);

    // Shader editor UI
    QDockWidget* shaderEditorDock = new QDockWidget(tr("Shader Editor"), this);
    shaderEditorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetClosable |
                                  QDockWidget::DockWidgetFloatable);
    QWidget* shaderEditorUI = new QWidget(shaderEditorDock);
    m_shaderEditor = new ShaderEditor(shaderEditorUI);
    QGridLayout* shaderEditorLayout = new QGridLayout(shaderEditorUI);
    shaderEditorLayout->setContentsMargins(2,2,2,2);
    shaderEditorLayout->addWidget(m_shaderEditor, 0, 0, 1, 1);
    connect(editShaderAct, SIGNAL(triggered()), shaderEditorDock, SLOT(show()));
    shaderEditorDock->setWidget(shaderEditorUI);

    shaderMenu->addAction(m_shaderEditor->compileAction());
    connect(m_shaderEditor->compileAction(), SIGNAL(triggered()),
            this, SLOT(compileShaderFile()));

    // TODO: check if this is needed - test shader update functionality
    //connect(m_shaderEditor, SIGNAL(sendShader(QString)),
    //        &m_pointView->shaderProgram(), SLOT(setShader(QString)));

    // Log viewer UI
    QDockWidget* logDock = new QDockWidget(tr("Log"), this);
    logDock->setFeatures(QDockWidget::DockWidgetMovable |
                         QDockWidget::DockWidgetClosable);
    QWidget* logUI = new QWidget(logDock);
    m_logTextView = new LogViewer(logUI);
    m_logTextView->setReadOnly(true);
    m_logTextView->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_logTextView->connectLogger(&g_logger); // connect to global logger
    m_progressBar = new QProgressBar(logUI);
    m_progressBar->setRange(0,100);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    connect(m_fileLoader, SIGNAL(loadStepStarted(QString)),
            this, SLOT(setProgressBarText(QString)));
    connect(m_fileLoader, SIGNAL(loadProgress(int)),
            m_progressBar, SLOT(setValue(int)));
    connect(m_fileLoader, SIGNAL(resetProgress()),
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
    DataSetUI* dataSetUI = new DataSetUI(this);
    dataSetDock->setWidget(dataSetUI);
    QAbstractItemView* dataSetOverview = dataSetUI->view();
    dataSetOverview->setModel(m_geometries);
    connect(dataSetOverview, SIGNAL(doubleClicked(const QModelIndex&)),
            m_pointView, SLOT(centerOnGeometry(const QModelIndex&)));
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
    viewMenu->addAction(logDock->toggleViewAction());
    viewMenu->addAction(dataSetDock->toggleViewAction());

    // Create custom hook events from CLI at runtime
    m_hookManager = new HookManager(this);
}


void PointViewerMainWindow::startIpcServer(const QString& socketName)
{
    delete m_ipcServer;
    m_ipcServer = new QLocalServer(this);
    if (!QLocalServer::removeServer(socketName))
        qWarning("Could not clean up socket file \"%s\"", qPrintable(socketName));
    if (!m_ipcServer->listen(socketName))
        qWarning("Could not listen on socket \"%s\"", qPrintable(socketName));
    connect(m_ipcServer, SIGNAL(newConnection()),
            this, SLOT(handleIpcConnection()));
}


void PointViewerMainWindow::handleIpcConnection()
{
    IpcChannel* channel = new IpcChannel(m_ipcServer->nextPendingConnection(), this);
    connect(channel, SIGNAL(disconnected()), channel, SLOT(deleteLater()));
    connect(channel, SIGNAL(messageReceived(QByteArray)), this, SLOT(handleMessage(QByteArray)));
}


void PointViewerMainWindow::setProgressBarText(QString text)
{
    m_progressBar->show();
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
    for (int i = 0; i < urls.size(); ++i)
    {
         if (urls[i].isLocalFile())
             m_fileLoader->loadFile(FileLoadInfo(urls[i].toLocalFile()));
    }
}


QSize PointViewerMainWindow::sizeHint() const
{
    return QSize(800,600);
}


void PointViewerMainWindow::handleMessage(QByteArray message)
{
    QList<QByteArray> commandTokens = message.split('\n');
    if (commandTokens.empty())
        return;
    if (commandTokens[0] == "OPEN_FILES")
    {
        QList<QByteArray> flags = commandTokens[1].split('\0');
        bool replaceLabel = flags.contains("REPLACE_LABEL");
        bool deleteAfterLoad = flags.contains("DELETE_AFTER_LOAD");
        bool mutateExisting = flags.contains("MUTATE_EXISTING");
        for (int i = 2; i < commandTokens.size(); ++i)
        {
            QList<QByteArray> pathAndLabel = commandTokens[i].split('\0');
            if (pathAndLabel.size() != 2)
            {
                g_logger.error("Unrecognized OPEN_FILES token: %s",
                               QString(commandTokens[i]));
                continue;
            }
            FileLoadInfo loadInfo(pathAndLabel[0], pathAndLabel[1], replaceLabel);
            loadInfo.deleteAfterLoad = deleteAfterLoad;
            loadInfo.mutateExisting = mutateExisting;
            m_fileLoader->loadFile(loadInfo);
        }
    }
    else if (commandTokens[0] == "CLEAR_FILES")
    {
        m_geometries->clear();
    }
    else if (commandTokens[0] == "UNLOAD_FILES")
    {
        QString regex_str = commandTokens[1];
        QRegExp regex(regex_str, Qt::CaseSensitive, QRegExp::WildcardUnix);
        if (!regex.isValid())
        {
            g_logger.error("Invalid pattern in -unload command: '%s': %s",
                           regex_str, regex.errorString());
            return;
        }
        m_geometries->unloadFiles(regex);
        m_pointView->removeAnnotations(regex);
    }
    else if (commandTokens[0] == "SET_VIEW_LABEL")
    {
        QString regex_str = commandTokens[1];
        QRegExp regex(regex_str, Qt::CaseSensitive, QRegExp::FixedString);
        if (!regex.isValid())
        {
            g_logger.error("Invalid pattern in -unload command: '%s': %s",
                           regex_str, regex.errorString());
            return;
        }
        QModelIndex index = m_geometries->findLabel(regex);
        if (index.isValid())
            m_pointView->centerOnGeometry(index);
    }
    else if (commandTokens[0] == "ANNOTATE")
    {
        if (commandTokens.size() - 1 != 5)
        {
            tfm::format(std::cerr, "Expected five arguments, got %d\n",
                        commandTokens.size() - 1);
            return;
        }
        QString label = commandTokens[1];
        QString text = commandTokens[2];
        bool xOk = false, yOk = false, zOk = false;
        double x = commandTokens[3].toDouble(&xOk);
        double y = commandTokens[4].toDouble(&yOk);
        double z = commandTokens[5].toDouble(&zOk);
        if (!zOk || !yOk || !zOk)
        {
            std::cerr << "Could not parse XYZ coordinates for position\n";
            return;
        }
        m_pointView->addAnnotation(label, text, Imath::V3d(x, y, z));
    }
    else if (commandTokens[0] == "SET_VIEW_POSITION")
    {
        if (commandTokens.size()-1 != 3)
        {
            tfm::format(std::cerr, "Expected three coordinates, got %d\n",
                        commandTokens.size()-1);
            return;
        }
        bool xOk = false, yOk = false, zOk = false;
        double x = commandTokens[1].toDouble(&xOk);
        double y = commandTokens[2].toDouble(&yOk);
        double z = commandTokens[3].toDouble(&zOk);
        if (!zOk || !yOk || !zOk)
        {
            std::cerr << "Could not parse XYZ coordinates for position\n";
            return;
        }
        m_pointView->setExplicitCursorPos(Imath::V3d(x, y, z));
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
    else if (commandTokens[0] == "SET_VIEW_ROTATION")
    {
        if (commandTokens.size()-1 != 9)
        {
            tfm::format(std::cerr, "Expected 9 rotation matrix components, got %d\n",
                        commandTokens.size()-1);
            return;
        }
#       ifdef DISPLAZ_USE_QT4
        qreal rot[9] = {0};
#       else
        float rot[9] = {0};
#       endif
        for (int i = 0; i < 9; ++i)
        {
            bool ok = true;
            rot[i] = commandTokens[i+1].toDouble(&ok);
            if(!ok)
            {
                tfm::format(std::cerr, "badly formatted view matrix message:\n%s", message.constData());
                return;
            }
        }
        m_pointView->camera().setRotation(QMatrix3x3(rot));
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
    else if (commandTokens[0] == "QUERY_CURSOR")
    {
        // Yuck!
        IpcChannel* channel = dynamic_cast<IpcChannel*>(sender());
        if (!channel)
        {
            qWarning() << "Signalling object not a IpcChannel!\n";
            return;
        }
        V3d p = m_pointView->cursorPos();
        std::string response = tfm::format("%.15g %.15g %.15g", p.x, p.y, p.z);
        channel->sendMessage(QByteArray(response.data(), (int)response.size()));
    }
    else if (commandTokens[0] == "QUIT")
    {
        close();
    }
    else if (commandTokens[0] == "SET_MAX_POINT_COUNT")
    {
        m_maxPointCount = commandTokens[1].toLongLong();
    }
    else if (commandTokens[0] == "OPEN_SHADER")
    {
        openShaderFile(commandTokens[1]);
    }
    else if (commandTokens[0] == "NOTIFY")
    {
        if (commandTokens.size() < 3)
        {
            g_logger.error("Could not parse NOTIFY message: %s", QString::fromUtf8(message));
            return;
        }
        QString spec = QString::fromUtf8(commandTokens[1]);
        QList<QString> specList = spec.split(':');
        if (specList[0].toLower() != "log")
        {
            g_logger.error("Could not parse NOTIFY spec: %s", spec);
            return;
        }

        Logger::LogLevel level = Logger::Info;
        if (specList.size() > 1)
            level = Logger::parseLogLevel(specList[1].toLower().toStdString());

        // Ugh, reassemble message from multiple lines.  Need a better
        // transport serialization!
        QByteArray message;
        for (int i = 2; i < commandTokens.size(); ++i)
        {
            if (i > 2)
                message += "\n";
            message += commandTokens[i];
        }

        g_logger.log(level, "%s", tfm::makeFormatList(message.constData()));
    }
    else if(commandTokens[0] == "HOOK")
    {
        IpcChannel* channel = dynamic_cast<IpcChannel*>(sender());
        if (!channel)
        {
            qWarning() << "Signalling object not a IpcChannel!\n";
            return;
        }
        for(int i=1; i<commandTokens.count(); i+=2)
        {
            HookFormatter* formatter = new HookFormatter(this, commandTokens[i], commandTokens[i+1], channel);
            m_hookManager->connectHook(commandTokens[i], formatter);
        }
    }
    else
    {
        g_logger.error("Unkown remote message:\n%s", QString::fromUtf8(message));
    }
}


QByteArray PointViewerMainWindow::hookPayload(QByteArray payload)
{
    if(payload == QByteArray("cursor"))
    {
        V3d p = m_pointView->cursorPos();
        std::string response = tfm::format("%.15g %.15g %.15g", p.x, p.y, p.z);
        return (payload + " " + QByteArray(response.data(), (int)response.size()));
    }
    else
    {
        return QByteArray("null");//payload;
    }
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
        tr("Data sets (*.las *.laz *.txt *.xyz *.ply);;All files (*)"),
        0,
        QFileDialog::ReadOnly
    );
    if (files.empty())
        return;
    for (int i = 0; i < files.size(); ++i)
        m_fileLoader->loadFile(FileLoadInfo(files[i]));
    m_currFileDir = QFileInfo(files[0]).dir();
    m_currFileDir.makeAbsolute();
}


void PointViewerMainWindow::addFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Add point clouds or meshes"),
        m_currFileDir.path(),
        tr("Data sets (*.las *.laz *.txt *.xyz *.ply);;All files (*)"),
        0,
        QFileDialog::ReadOnly
    );
    if (files.empty())
        return;
    for (int i = 0; i < files.size(); ++i)
    {
        FileLoadInfo loadInfo(files[i]);
        loadInfo.replaceLabel = false;
        m_fileLoader->loadFile(loadInfo);
    }
    m_currFileDir = QFileInfo(files[0]).dir();
    m_currFileDir.makeAbsolute();
}

void PointViewerMainWindow::openShaderFile(const QString& shaderFileName)
{
    QFile shaderFile(shaderFileName);
    if (!shaderFile.open(QIODevice::ReadOnly))
    {
        shaderFile.setFileName("shaders:" + shaderFileName);
        if (!shaderFile.open(QIODevice::ReadOnly))
        {
            g_logger.error("Couldn't open shader file \"%s\": %s",
                        shaderFileName, shaderFile.errorString());
            return;
        }
    }
    m_currShaderFileName = shaderFile.fileName();
    QByteArray src = shaderFile.readAll();
    m_shaderEditor->setPlainText(src);
    m_pointView->shaderProgram().setShader(src);
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


void PointViewerMainWindow::compileShaderFile()
{
    m_pointView->shaderProgram().setShader(m_shaderEditor->toPlainText());
}


void PointViewerMainWindow::reloadFiles()
{
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    for (auto g = geoms.begin(); g != geoms.end(); ++g)
    {
        FileLoadInfo loadInfo((*g)->fileName(), (*g)->label(), false);
        m_fileLoader->reloadFile(loadInfo);
    }
}


void PointViewerMainWindow::helpDialog()
{
    m_helpDialog->show();
}


void PointViewerMainWindow::screenShot()
{
    // Hack: do the grab first, before the widget is covered by the save
    // dialog.
    //
    // Grabbing the desktop directly using grabWindow() isn't great, but makes
    // this much simpler to implement.  (Other option: use
    // m_pointView->renderPixmap() and turn off incremental rendering.)
    QPoint tl = m_pointView->mapToGlobal(QPoint(0,0));
    QPixmap sshot = QPixmap::grabWindow(QApplication::desktop()->winId(),
                                        tl.x(), tl.y(),
                                        m_pointView->width(), m_pointView->height());
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save screen shot"),
        QDir::currentPath(),
        tr("Image files (*.tif *.png *.jpg);;All files(*)")
    );
    if (fileName.isNull())
        return;
    sshot.save(fileName);
}


void PointViewerMainWindow::aboutDialog()
{
    QString message = tr(
        "<p><a href=\"http://c42f.github.io/displaz\"><b>Displaz</b></a> &mdash; a viewer for lidar point clouds</p>"
        "<p>Version " DISPLAZ_VERSION_STRING "</p>"
        "<p>This software is open source under the BSD 3-clause license.  "
        "Source code is available at <a href=\"https://github.com/c42f/displaz\">https://github.com/c42f/displaz</a>.</p>"
    );
    QMessageBox::information(this, tr("About displaz"), message);
}


void PointViewerMainWindow::setBackground(const QString& name)
{
    m_pointView->setBackground(QColor(name));
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
    QStringList labels;
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    int numGeoms = 0;
    for (auto i = geoms.begin(); i != geoms.end(); ++i)
    {
        labels << (*i)->label();
        numGeoms += 1;
        if (numGeoms > 10)
        {
            labels << "...";
            break;
        }
    }
    setWindowTitle(tr("Displaz - %1").arg(labels.join(", ")));
}
