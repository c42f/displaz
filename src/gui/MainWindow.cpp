// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "MainWindow.h"

#include "config.h"
#include "DataSetUI.h"
#include "fileloader.h"
#include "geometrycollection.h"
#include "HelpDialog.h"
#include "IpcChannel.h"
#include "QtLogger.h"
#include "TriMesh.h"
#include "Enable.h"
#include "ShaderEditor.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include "View3D.h"
#include "HookFormatter.h"
#include "HookManager.h"

#include <QSignalMapper>
#include <QThread>
#include <QScreen>
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
#include <QStatusBar>
#include <QDoubleSpinBox>
#include <QDesktopWidget>
#include <QDropEvent>
#include <QLocalServer>
#include <QMimeData>
#include <QGLFormat>

//------------------------------------------------------------------------------
// MainWindow implementation

MainWindow::MainWindow(const QGLFormat& format)
    : m_settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName()),
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

    // Actions
    //
    // These actions work whether or not the toolbar and/or menu is active or shown

    m_open = new QAction(tr("&Open"), this);
    m_open->setToolTip(tr("Open a data set"));
    m_open->setShortcuts(QKeySequence::Open);
    connect(m_open, SIGNAL(triggered()), this, SLOT(openFiles()));

    m_screenShot = new QAction(tr("Scree&nshot"), this);
    m_screenShot->setStatusTip(tr("Save screen shot of 3D window"));
    m_screenShot->setShortcut(Qt::Key_F9);
    connect(m_screenShot, SIGNAL(triggered()), this, SLOT(screenShot()));

    m_quit = new QAction(tr("&Quit"), this);
    m_quit->setStatusTip(tr("Exit the application"));
    m_quit->setCheckable(false);
    m_quit->setShortcut(Qt::CTRL | Qt::Key_Q);
    m_quit->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_quit, SIGNAL(triggered()), this, SLOT(close()));
    addAction(m_quit);

    // For Windows ALT+F4 exit
    m_quitGeneric = new QAction(tr("&Quit"), this);
    m_quitGeneric->setStatusTip(tr("Exit the application"));
    m_quitGeneric->setCheckable(false);
    m_quitGeneric->setShortcut(QKeySequence::Quit);
    m_quitGeneric->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_quitGeneric, SIGNAL(triggered()), this, SLOT(close()));
    addAction(m_quitGeneric);

    m_fullScreen = new QAction(tr("&Full Screen"), this);
    m_fullScreen->setStatusTip(tr("Toggle Full Screen Mode"));
    m_fullScreen->setCheckable(true);
    m_fullScreen->setShortcut(Qt::Key_F11);
    m_fullScreen->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_fullScreen, SIGNAL(triggered()), this, SLOT(fullScreen()));
    addAction(m_fullScreen);

    m_trackBall = new QAction(tr("Use &Trackball camera"), this);
    m_trackBall->setStatusTip(tr("Toggle Trackball Mode"));
    m_trackBall->setCheckable(true);
    m_trackBall->setChecked(true);
    addAction(m_trackBall);

    // Menus
    menuBar()->setNativeMenuBar(false); // OS X doesn't activate the native menu bar under Qt5

    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_open);
    QAction* addAct = fileMenu->addAction(tr("&Add"));
    addAct->setToolTip(tr("Add a data set"));
    connect(addAct, SIGNAL(triggered()), this, SLOT(addFiles()));

    QAction* reloadAction = fileMenu->addAction(tr("&Reload"));
    reloadAction->setStatusTip(tr("Reload point files from disk"));
    reloadAction->setShortcut(Qt::Key_F5);
    connect(reloadAction, SIGNAL(triggered()), this, SLOT(reloadFiles()));

    fileMenu->addSeparator();
    fileMenu->addAction(m_screenShot);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quit);

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_fullScreen);
    viewMenu->addAction(m_trackBall);
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
    connect(m_trackBall, SIGNAL(triggered(bool)),
            &(m_pointView->camera()), SLOT(setTrackballInteraction(bool)));
    connect(m_geometries, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(geometryRowsInserted(QModelIndex,int,int)));

    // Status bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0,100);
    m_progressBar->setValue(0);
    m_progressBar->hide();
    statusBar()->addPermanentWidget(m_progressBar);
    connect(m_fileLoader, SIGNAL(loadStepStarted(QString)),
            this, SLOT(loadStepStarted(QString)));
    connect(m_fileLoader, SIGNAL(loadProgress(int)),
            m_progressBar, SLOT(setValue(int)));
    connect(m_fileLoader, SIGNAL(loadStepComplete()),
            this, SLOT(loadStepComplete()));

    // Check boxes for drawing various scene elements by category
    viewMenu->addSeparator();
    viewMenu->addAction(m_pointView->m_boundingBoxAction);
    viewMenu->addAction(m_pointView->m_cursorAction);
    viewMenu->addAction(m_pointView->m_axesAction);
    viewMenu->addAction(m_pointView->m_gridAction);
    viewMenu->addAction(m_pointView->m_annotationAction);

    //--------------------------------------------------
    // Docked widgets
    // Shader parameters UI
    m_dockShaderParameters = new QDockWidget(tr("Shader Parameters"), this);
    m_dockShaderParameters->setObjectName("ShaderParameters");
    m_dockShaderParameters->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetClosable);
    QWidget* shaderParamsUI = new QWidget(m_dockShaderParameters);
    m_dockShaderParameters->setWidget(shaderParamsUI);
    m_pointView->setShaderParamsUIWidget(shaderParamsUI);

    // Shader editor UI
    m_dockShaderEditor = new QDockWidget(tr("Shader Editor"), this);
    m_dockShaderEditor->setObjectName("ShaderEditor");
    m_dockShaderEditor->setFeatures(QDockWidget::DockWidgetMovable |
                                  QDockWidget::DockWidgetClosable |
                                  QDockWidget::DockWidgetFloatable);
    QWidget* shaderEditorUI = new QWidget(m_dockShaderEditor);
    m_shaderEditor = new ShaderEditor(shaderEditorUI);
    QGridLayout* shaderEditorLayout = new QGridLayout(shaderEditorUI);
    shaderEditorLayout->setContentsMargins(2,2,2,2);
    shaderEditorLayout->addWidget(m_shaderEditor, 0, 0, 1, 1);
    connect(editShaderAct, SIGNAL(triggered()), m_dockShaderEditor, SLOT(show()));
    m_dockShaderEditor->setWidget(shaderEditorUI);

    shaderMenu->addAction(m_shaderEditor->compileAction());
    connect(m_shaderEditor->compileAction(), SIGNAL(triggered()),
            this, SLOT(compileShaderFile()));

    // TODO: check if this is needed - test shader update functionality
    //connect(m_shaderEditor, SIGNAL(sendShader(QString)),
    //        &m_pointView->shaderProgram(), SLOT(setShader(QString)));

    // Log viewer UI
    m_dockLog = new QDockWidget(tr("Log"), this);
    m_dockLog->setObjectName("Log");
    m_dockLog->setFeatures(QDockWidget::DockWidgetMovable |
                         QDockWidget::DockWidgetClosable);
    QWidget* logUI = new QWidget(m_dockLog);
    m_logTextView = new LogViewer(logUI);
    m_logTextView->setReadOnly(true);
    m_logTextView->setTextInteractionFlags(Qt::TextSelectableByKeyboard | Qt::TextSelectableByMouse);
    m_logTextView->connectLogger(&g_logger); // connect to global logger
    QVBoxLayout* logUILayout = new QVBoxLayout(logUI);
    //logUILayout->setContentsMargins(2,2,2,2);
    logUILayout->addWidget(m_logTextView);
    //m_logTextView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_dockLog->setWidget(logUI);

    // Data set list UI
    m_dockDataSet = new QDockWidget(tr("Data Sets"), this);
    m_dockDataSet->setObjectName("DataSets");
    m_dockDataSet->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetClosable |
                              QDockWidget::DockWidgetFloatable);
    DataSetUI* dataSetUI = new DataSetUI(this);
    m_dockDataSet->setWidget(dataSetUI);
    connect(dataSetUI->view(), SIGNAL(reloadFile(const QModelIndex&)),
            this, SLOT(reloadFile(const QModelIndex&)));

    QAbstractItemView* dataSetOverview = dataSetUI->view();
    dataSetOverview->setModel(m_geometries);
    connect(dataSetOverview, SIGNAL(doubleClicked(const QModelIndex&)),
            m_pointView, SLOT(centerOnGeometry(const QModelIndex&)));
    m_pointView->setSelectionModel(dataSetOverview->selectionModel());

    // Set up docked widgets
    addDockWidget(Qt::RightDockWidgetArea, m_dockShaderParameters);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockShaderEditor);
    addDockWidget(Qt::RightDockWidgetArea, m_dockLog);
    addDockWidget(Qt::RightDockWidgetArea, m_dockDataSet);
    tabifyDockWidget(m_dockLog, m_dockDataSet);
    m_dockLog->raise();
    m_dockShaderEditor->setVisible(false);

    // Add dock widget toggles to view menu
    viewMenu->addSeparator();
    viewMenu->addAction(m_dockShaderParameters->toggleViewAction());
    viewMenu->addAction(m_dockLog->toggleViewAction());
    viewMenu->addAction(m_dockDataSet->toggleViewAction());

    // Create custom hook events from CLI at runtime
    m_hookManager = new HookManager(this);

    readSettings();
}

void MainWindow::startIpcServer(const QString& socketName)
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


void MainWindow::handleIpcConnection()
{
    IpcChannel* channel = new IpcChannel(m_ipcServer->nextPendingConnection(), this);
    connect(channel, SIGNAL(disconnected()), channel, SLOT(deleteLater()));
    connect(channel, SIGNAL(messageReceived(QByteArray)), this, SLOT(handleMessage(QByteArray)));
}


void MainWindow::geometryRowsInserted(const QModelIndex& parent, int first, int last)
{
    QItemSelection range(m_geometries->index(first), m_geometries->index(last));
    m_pointView->selectionModel()->select(range, QItemSelectionModel::Select);
}


void MainWindow::dragEnterEvent(QDragEnterEvent *event)
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


void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;
    for (int i = 0; i < urls.size(); ++i)
    {
         if (urls[i].isLocalFile())
         {
             const QString filename = urls[i].toLocalFile();
             const QDir dir = QFileInfo(filename).absoluteDir();
             m_settings.setValue("lastDirectory", dir.path());
             m_fileLoader->loadFile(FileLoadInfo(filename));
         }
    }
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}


QSize MainWindow::sizeHint() const
{
    return QSize(800,600);
}


void MainWindow::handleMessage(QByteArray message)
{
    QList<QByteArray> commandTokens = message.split('\n');
    if (commandTokens.empty())
    {
        return;
    }
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
        float rot[9] = {0};
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


QByteArray MainWindow::hookPayload(QByteArray payload)
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

void MainWindow::fullScreen()
{
    const bool fullScreen = isFullScreen();

    // Remember which docks are visible, we'll hide them in fullscreen mode
    if (!fullScreen)
    {
        m_dockShaderEditorVisible     = m_dockShaderEditor->isVisible();
        m_dockShaderParametersVisible = m_dockShaderParameters->isVisible();
        m_dockDataSetVisible          = m_dockDataSet->isVisible();
        m_dockLogVisible              = m_dockLog->isVisible();
    }

    setWindowState(windowState() ^ Qt::WindowFullScreen);

    // Set the visibility of menu bar, status bar and docks

    menuBar()->setVisible(fullScreen);
    statusBar()->setVisible(fullScreen);

    m_dockShaderEditor    ->setVisible(fullScreen && m_dockShaderEditorVisible);
    m_dockShaderParameters->setVisible(fullScreen && m_dockShaderParametersVisible);
    m_dockDataSet         ->setVisible(fullScreen && m_dockDataSetVisible);
    m_dockLog             ->setVisible(fullScreen && m_dockLogVisible);
}

void MainWindow::openFiles()
{
    const QString lastDirectory = m_settings.value("lastDirectory").toString();

    // Note - using the static getOpenFileNames seems to be the only way to get
    // a native dialog.  This is annoying since the last selected directory
    // can't be deduced directly from the native dialog, but only when it
    // returns a valid selected file.  However we'll just have to put up with
    // this, because not having native dialogs is jarring.
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Open point clouds or meshes"),
        lastDirectory,
        tr("Data sets (*.las *.laz *.txt *.xyz *.ply);;LAZ Point Cloud (*.las *.laz *.slaz);;All files (*)"),
        0,
        QFileDialog::ReadOnly
    );
    for (int i = 0; i < files.size(); ++i)
    {
        const QString filename = files[i];
        QDir dir = QFileInfo(filename).dir();
        dir.makeAbsolute();
        m_settings.setValue("lastDirectory", dir.path());
        m_fileLoader->loadFile(FileLoadInfo(filename));
    }
}


void MainWindow::addFiles()
{
    const QString lastDirectory = m_settings.value("lastDirectory").toString();

    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Add point clouds or meshes"),
        lastDirectory,
        tr("Data sets (*.las *.laz *.txt *.xyz *.ply);;LAZ Point Cloud (*.las *.laz *.slaz);;All files (*)"),
        0,
        QFileDialog::ReadOnly
    );
    for (int i = 0; i < files.size(); ++i)
    {
        const QString filename = files[i];
        QDir dir = QFileInfo(filename).dir();
        dir.makeAbsolute();
        m_settings.setValue("lastDirectory", dir.path());
        FileLoadInfo loadInfo(filename);
        loadInfo.replaceLabel = false;
        m_fileLoader->loadFile(loadInfo);
    }
}

void MainWindow::openShaderFile(const QString& shaderFileName)
{
    QString filename(shaderFileName);

    if (filename.isNull())
    {
        filename = m_settings.value("lastShader").toString();
        if (filename.isNull())
        {
            filename = "shaders:las_points.glsl";
        }
    }

    QFile shaderFile(filename);

    if (!shaderFile.open(QIODevice::ReadOnly))
    {
        shaderFile.setFileName("shaders:" + filename);
        if (!shaderFile.open(QIODevice::ReadOnly))
        {
            g_logger.error("Couldn't open shader file \"%s\": %s",
                        filename, shaderFile.errorString());
            return;
        }
    }
    m_currShaderFileName = shaderFile.fileName();
    QByteArray src = shaderFile.readAll();
    m_shaderEditor->setPlainText(src);
    m_pointView->shaderProgram().setShader(src);
    m_pointView->enable().set(src);
    m_settings.setValue("lastShader", m_currShaderFileName);
}

void MainWindow::openShaderFile()
{
    const QString lastDirectory = m_settings.value("lastShaderDirectory").toString();

    QString shaderFileName = QFileDialog::getOpenFileName(
        this,
        tr("Open OpenGL shader in displaz format"),
        lastDirectory,
        tr("OpenGL shader files (*.glsl);;All files(*)")
    );
    if (shaderFileName.isNull())
        return;

    QDir dir = QFileInfo(shaderFileName).dir();
    m_settings.setValue("lastShaderDirectory", dir.path());
    openShaderFile(shaderFileName);
}


void MainWindow::saveShaderFile()
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


void MainWindow::compileShaderFile()
{
    m_pointView->shaderProgram().setShader(m_shaderEditor->toPlainText());
    m_pointView->enable().set(m_shaderEditor->toPlainText());
}


void MainWindow::reloadFiles()
{
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    for (auto g = geoms.begin(); g != geoms.end(); ++g)
    {
        FileLoadInfo loadInfo((*g)->fileName(), (*g)->label(), false);
        m_fileLoader->reloadFile(loadInfo);
    }
}

void MainWindow::reloadFile(const QModelIndex& index)
{
    const int row = index.row();
    const GeometryCollection::GeometryVec& geoms = m_geometries->get();
    if (row < static_cast<int>(geoms.size()))
    {
        FileLoadInfo loadInfo(geoms[row]->fileName(), geoms[row]->label(), false);
        m_fileLoader->reloadFile(loadInfo);
    }
}

void MainWindow::helpDialog()
{
    m_helpDialog->show();
}


void MainWindow::screenShot()
{
    const QString screenShotDirectory = m_settings.value("screenShotDirectory").toString();

    // Hack: do the grab first, before the widget is covered by the save
    // dialog.
    //
    // Grabbing the desktop directly using grabWindow() isn't great, but makes
    // this much simpler to implement.  (Other option: use
    // m_pointView->renderPixmap() and turn off incremental rendering.)
    QPixmap sshot = grab();
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save screen shot"),
        screenShotDirectory,
        tr("Image files (*.tif *.png *.jpg);;All files(*)")
    );
    if (!fileName.isNull())
    {
        QDir dir = QFileInfo(fileName).dir();
        dir.makeAbsolute();
        m_settings.setValue("screenShotDirectory", dir.path());
        sshot.save(fileName);
    }
}


void MainWindow::aboutDialog()
{
    QString message = tr(
        "<p><a href=\"http://c42f.github.io/displaz\"><b>Displaz</b></a> &mdash; a viewer for lidar point clouds</p>"
        "<p>Version " DISPLAZ_VERSION_STRING "</p>"
        "<p>This software is open source under the BSD 3-clause license.  "
        "Source code is available at <a href=\"https://github.com/c42f/displaz\">https://github.com/c42f/displaz</a>.</p>"
    );
    QMessageBox::information(this, tr("About displaz"), message);
}


void MainWindow::setBackground(const QString& name)
{
    m_pointView->setBackground(QColor(name));
}


void MainWindow::chooseBackground()
{
    QColor originalColor = m_pointView->background();
    QColorDialog chooser(originalColor, this);
    connect(&chooser, SIGNAL(currentColorChanged(QColor)),
            m_pointView, SLOT(setBackground(QColor)));
    if (chooser.exec() == QDialog::Rejected)
        m_pointView->setBackground(originalColor);
}


void MainWindow::updateTitle()
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

void MainWindow::loadStepStarted(const QString& description)
{
    statusBar()->showMessage(description, 5000);
    m_progressBar->show();
    m_progressBar->setFormat(description + " (%p%)");
}

void MainWindow::loadStepComplete()
{
    statusBar()->clearMessage();
    m_progressBar->hide();
}

void MainWindow::readSettings()
{
    restoreGeometry(m_settings.value("geometry").toByteArray());
    restoreState(m_settings.value("windowState").toByteArray());

    if (m_settings.value("minimised").toBool())
    {
        showMinimized();
    }

    // Hide menu bar and status bar in full screen mode
    const bool fullScreen = isFullScreen();
    menuBar()->setVisible(!fullScreen);
    statusBar()->setVisible(!fullScreen);

    const bool trackBall = m_settings.value("trackBall").toBool();
    m_trackBall->setChecked(trackBall);
    m_pointView->camera().setTrackballInteraction(trackBall);

    m_dockShaderEditorVisible     = m_settings.value("shaderEditor").toBool();
    m_dockShaderParametersVisible = m_settings.value("shaderParameters").toBool();
    m_dockDataSetVisible          = m_settings.value("dataSet").toBool();
    m_dockLogVisible              = m_settings.value("log").toBool();

    m_settings.beginGroup("view");
    m_pointView->readSettings(m_settings);
    m_settings.endGroup();
}

void MainWindow::writeSettings()
{
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("windowState", saveState());
    m_settings.setValue("minimised", isMinimized());
    m_settings.setValue("trackBall", m_trackBall->isChecked());

    m_settings.setValue("shaderEditor",     m_dockShaderEditorVisible);
    m_settings.setValue("shaderParameters", m_dockShaderParametersVisible);
    m_settings.setValue("dataSet",          m_dockDataSetVisible);
    m_settings.setValue("log",              m_dockLogVisible);

    m_settings.beginGroup("view");
    m_pointView->writeSettings(m_settings);
    m_settings.endGroup();
}
