// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

//#include <QtCore/QDataStream>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QTextCodec>

#include "argparse.h"
#include "config.h"
#include "IpcChannel.h"
#include "InterProcessLock.h"
#include "util.h"

//------------------------------------------------------------------------------

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
    bool printVersion = false;
    bool printHelp = false;
    int maxPointCount = -1;
    std::string serverName = "default";
    double yaw = -DBL_MAX, pitch = -DBL_MAX, roll = -DBL_MAX;
    double viewRadius = -DBL_MAX;

    std::string shaderName;
    bool useServer = true;  // FIXME!

    bool clearFiles = false;
    bool addFiles = false;
    bool rmTemp = false;
    bool quitRemote = false;
    bool queryCursor = false;
    bool background = false;

    ArgParse::ArgParse ap;
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
        "-background",   &background,    "Put process in background - do not wait for displaz GUI to exit",

        "<SEPARATOR>", "\nAdditional information:",
        "-version",      &printVersion,  "Print version number",
        "-help",         &printHelp,     "Print command line usage help",
        NULL
    );

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

    // Use QCoreApplication rather than QApplication here since it doesn't
    // require GUI resources which can get exhaused if a lot of instances are
    // started at once.
    QCoreApplication application(argc, argv);
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));

    std::string ipcResourceName = displazIpcName(serverName);
    QString socketName = QString::fromStdString(ipcResourceName);

    std::string lockName = ipcResourceName + ".lock";
    InterProcessLock instanceLock(lockName);
    bool startedGui = false;
    qint64 guiPid = -1;
    if (!useServer || instanceLock.tryLock())
    {
        // Check that the user hasn't made a mess of the command line options
        if (queryCursor)
        {
            std::cerr << "ERROR: No remote displaz instance found\n";
            return EXIT_FAILURE;
        }
        if (quitRemote || clearFiles)
        {
            return EXIT_SUCCESS;
        }

        // Launch the main GUI window in a separate process.
        QStringList args;
        if (useServer)
        {
            args << "-instancelock" << QString::fromStdString(lockName)
                                    << QString::fromStdString(instanceLock.makeLockId())
                 << "-socketname"   << socketName;
        }
        QString guiExe = QDir(QCoreApplication::applicationDirPath())
                         .absoluteFilePath("displaz-gui");
        if (!QProcess::startDetached(guiExe, args,
                                     QDir::currentPath(), &guiPid))
        {
            std::cerr << "ERROR: Could not start remote displaz process\n";
            return EXIT_FAILURE;
        }
        startedGui = true;
    }

    // Remote displaz instance should now be running (either it existed
    // already, or we started it above).  Communicate with it via the socket
    // interface to set any requested parameters, load additional files etc.
    std::unique_ptr<IpcChannel> channel = IpcChannel::connectToServer(socketName);
    if (!channel)
    {
        std::cerr << "ERROR: Could not open IPC channel to remote instance - exiting\n";
        return EXIT_FAILURE;
    }

    // Format IPC message, or exit if none specified.
    //
    // TODO: Factor out this socket comms code - sending and recieving of
    // messages should happen in a centralised place.
    if (!g_initialFileNames.empty())
    {
        QByteArray command;
        QDir currentDir = QDir::current();
        command = addFiles ? "ADD_FILES" : "OPEN_FILES";
        if (rmTemp)
            command += "\nRMTEMP";
        for (int i = 0; i < g_initialFileNames.size(); ++i)
        {
            command += "\n";
            command += currentDir.absoluteFilePath(g_initialFileNames[i]).toUtf8();
        }
        channel->sendMessage(command);
    }
    if (clearFiles)
    {
        channel->sendMessage("CLEAR_FILES");
    }
    if (yaw != -DBL_MAX)
    {
        channel->sendMessage("SET_VIEW_ANGLES\n" +
                             QByteArray().setNum(yaw)   + "\n" +
                             QByteArray().setNum(pitch) + "\n" +
                             QByteArray().setNum(roll));
    }
    if (viewRadius != -DBL_MAX)
    {
        channel->sendMessage("SET_VIEW_RADIUS\n" +
                             QByteArray().setNum(viewRadius));
    }
    if (quitRemote)
    {
        channel->sendMessage("QUIT");
    }
    if (queryCursor)
    {
        channel->sendMessage("QUERY_CURSOR");
        QByteArray msg = channel->receiveMessage();
        std::cout.write(msg.data(), msg.length());
        std::cout << "\n";
    }
    if (maxPointCount > 0)
    {
        channel->sendMessage("SET_MAX_POINT_COUNT\n" +
                             QByteArray().setNum(maxPointCount));
    }
    if (!shaderName.empty() && startedGui)
    {
        // Note - only send the OPEN_SHADER command when the GUI is initially
        // started, since this is probably what you want when using from a
        // scripting language.
        // TODO: This shader open behaviour seems a bit inconsistent - figure
        // out how to make it nicer?
        channel->sendMessage(QByteArray("OPEN_SHADER\n") +
                             shaderName.c_str());
    }

    if (startedGui && !background)
    {
        SigIntTransferHandler sigIntHandler(guiPid);
        channel->waitForDisconnected(-1);
    }
    else
    {
        channel->disconnectFromServer();
    }

    return EXIT_SUCCESS;
}

