// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "mainwindow.h"
#include "geometrycollection.h"

#include <QtCore/QDataStream>
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtOpenGL/QGLFormat>

#include "argparse.h"
#include "config.h"
#include "fileloader.h"
#include "IpcChannel.h"
#include "InterProcessLock.h"
#include "util.h"

class Geometry;


/// Set up search paths to our application directory for Qt's file search
/// mechanism.
///
/// This allows us to use "shaders:las_points.glsl" as a path to a shader
/// in the rest of the code, regardless of the system-specific details of how
/// the install directories are laid out.
static void setupQFileSearchPaths()
{
    QString installBinDir = QCoreApplication::applicationDirPath();
    if (!installBinDir.endsWith("/bin"))
    {
        std::cerr << "WARNING: strange install location detected "
                     "- shaders will not be found\n";
        return;
    }
    QString installBaseDir = installBinDir;
    installBaseDir.chop(4);
    QDir::addSearchPath("shaders", installBaseDir + "/" + DISPLAZ_SHADER_DIR);
    QDir::addSearchPath("doc", installBaseDir + "/" + DISPLAZ_DOC_DIR);
}


/// Get resource name for displaz IPC
///
/// This is a combination of the program name and user name to avoid any name
/// clashes, along with a user-defined suffix.
static std::string displazIpcName(const std::string& suffix = "")
{
    std::string name = "displaz-ipc-" + currentUserUid();
    if (!suffix.empty())
        name += "-" + suffix;
    return name;
}


/// Run the main GUI window event loop
int runGuiEventLoop(int argc, char **argv,
                    bool useServer, QString socketName,
                    int maxPointCount, QString shaderName,
                    QStringList initialFileNames,
                    bool removeInitialFiles)
{
    QApplication app(argc, argv);

    setupQFileSearchPaths();

    Q_INIT_RESOURCE(resource);

    qRegisterMetaType<std::shared_ptr<Geometry>>("std::shared_ptr<Geometry>");

    // Multisampled antialiasing - this makes rendered point clouds look much
    // nicer, but also makes the render much slower, especially on lower
    // powered graphics cards.
    //QGLFormat f = QGLFormat::defaultFormat();
    //f.setSampleBuffers(true);
    //QGLFormat::setDefaultFormat(f);

    PointViewerMainWindow window;
    window.setMaxPointCount(maxPointCount);
    if (useServer)
        window.startIpcServer(socketName);
    if (!shaderName.isEmpty())
        window.openShaderFile(shaderName);
    window.show();
    for (int i = 0; i < initialFileNames.size(); ++i)
        window.fileLoader().loadFile(initialFileNames[i], removeInitialFiles);

    return app.exec();
}


/// Callback for argument parsing
static QStringList g_initialFileNames;
static int storeFileName (int argc, const char *argv[])
{
    for(int i = 0; i < argc; ++i)
        g_initialFileNames.push_back (argv[i]);
    return 0;
}


int main(int argc, char *argv[])
{
    ArgParse::ArgParse ap;
    bool printVersion = false;
    bool printHelp = false;
    int maxPointCount = 200000000;
    std::string serverName = "default";
    double yaw = -DBL_MAX, pitch = -DBL_MAX, roll = -DBL_MAX;
    double viewRadius = -DBL_MAX;

    std::string shaderName;
    bool useServer = true;

    bool clearFiles = false;
    bool addFiles = false;
    bool rmTemp = false;
    bool quitRemote = false;
    bool queryCursor = false;

    ap.options(
        "displaz - A lidar point cloud viewer\n"
        "Usage: displaz [opts] [file1.las ...]",
        "%*", storeFileName, "",

        "<SEPARATOR>", "\nInitial settings / remote commands:",
        "-maxpoints %d", &maxPointCount, "Maximum number of points to load at a time",
        "-noserver %!",  &useServer,     "Don't attempt to open files in existing window",
        "-server %s",    &serverName,    "Name of displaz instance to message on startup",
        "-shader %s",    &shaderName,    "Name of shader file to load on startup",
        "-viewangles %F %F %F", &yaw, &pitch, &roll, "Set view angles in degrees [yaw, pitch, roll]",
        "-viewradius %F", &viewRadius,   "Set distance to view point",
        "-clear",        &clearFiles,    "Remote: clear all currently loaded files",
        "-quit",         &quitRemote,    "Remote: close the existing displaz window",
        "-add",          &addFiles,      "Remote: add files to currently open set",
        "-rmtemp",       &rmTemp,        "*Delete* files after loading - use with caution to clean up single-use temporary files after loading",
        "-querycursor",  &queryCursor,   "Query 3D cursor location from displaz instance",

        "<SEPARATOR>", "\nAdditional information:",
        "-version",      &printVersion,  "Print version number",
        "-help",         &printHelp,     "Print command line usage help",
        NULL
    );

    attachToParentConsole();
    if(ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        ap.usage();
        std::cerr << "ERROR: " << ap.geterror() << std::endl;
        return EXIT_FAILURE;
    }

    if (printVersion)
    {
        std::cout << "version " DISPLAZ_VERSION_STRING "\n";
        return EXIT_SUCCESS;
    }
    if (printHelp)
    {
        ap.usage();
        return EXIT_SUCCESS;
    }

    if (!useServer && (quitRemote || queryCursor || clearFiles))
    {
        std::cerr << "ERROR: given commands cannot be combined with -noserver\n";
        return EXIT_FAILURE;
    }

    std::string ipcResourceName = displazIpcName(serverName);
    QString socketName = QString::fromStdString(ipcResourceName);

    InterProcessLock instanceLock(ipcResourceName + ".lock");
    if (!useServer || instanceLock.tryLock())
    {
        // Current process should act as the main GUI window, exiting when it
        // closes.

        // First check that the user hasn't made a mess of the command line
        // options:
        if (queryCursor)
        {
            std::cerr << "ERROR: No remote displaz instance found\n";
            return EXIT_FAILURE;
        }
        if (quitRemote || clearFiles)
        {
            return EXIT_SUCCESS;
        }

        return runGuiEventLoop(argc, argv, useServer, socketName,
                               maxPointCount, QString::fromStdString(shaderName),
                               g_initialFileNames, rmTemp);
    }

    // Another displaz instance is running - try to communicate with it.

    // Format IPC message, or exit if none specified.
    //
    // TODO: Factor out this socket comms code - sending and recieving of
    // messages should happen in a centralised place.
    QByteArray command;
    if (!g_initialFileNames.empty())
    {
        QDir currentDir = QDir::current();
        command = addFiles ? "ADD_FILES" : "OPEN_FILES";
        if (rmTemp)
            command += "\nRMTEMP";
        for (int i = 0; i < g_initialFileNames.size(); ++i)
        {
            command += "\n";
            command += currentDir.absoluteFilePath(g_initialFileNames[i]).toUtf8();
        }
    }
    else if (clearFiles)
    {
        command = "CLEAR_FILES";
    }
    else if (yaw != -DBL_MAX)
    {
        command = "SET_VIEW_ANGLES\n" +
                  QByteArray().setNum(yaw)   + "\n" +
                  QByteArray().setNum(pitch) + "\n" +
                  QByteArray().setNum(roll);
    }
    else if (viewRadius != -DBL_MAX)
    {
        command = "SET_VIEW_RADIUS\n" +
                  QByteArray().setNum(viewRadius);
    }
    else if (quitRemote)
    {
        command = "QUIT";
    }
    else if (queryCursor)
    {
        command = "QUERY_CURSOR";
    }
    else
    {
        std::cerr << "WARNING: Existing window found, but no remote "
                        "command specified - exiting\n";
        return EXIT_FAILURE;
    }

    // Using Qt sockets requires QCoreApplication or QApplication to be
    // instantiated.  Create a QCoreApplication since it doesn't require GUI
    // resources which can get exhaused if a lot of instances go down this code
    // path.
    QCoreApplication app(argc, argv);

    // Attempt to locate a running displaz instance
    std::unique_ptr<IpcChannel> channel = IpcChannel::connectToServer(socketName);
    if (!channel)
    {
        std::cerr << "ERROR: Could not open IPC channel to remote instance - exiting\n";
        return EXIT_FAILURE;
    }
    channel->sendMessage(command);
    if (queryCursor)
    {
        QByteArray msg = channel->receiveMessage();
        std::cout.write(msg.data(), msg.length());
        std::cout << "\n";
    }
    channel->disconnectFromServer();

    return EXIT_SUCCESS;
}

