// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

//#include <QDataStream>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QUuid>

#include <cfloat>

#include "argparse.h"
#include "config.h"
#include "IpcChannel.h"
#include "InterProcessLock.h"
#include "util.h"
#include "guimain.h"

//------------------------------------------------------------------------------

/// Callback and globals for positional argument parsing
struct PositionalArg
{
    std::string filePath;
    std::string dataSetLabel;
};

static std::string g_dataSetLabel;
static std::vector<PositionalArg> g_initialFileNames;
static int storeFileName (int argc, const char *argv[])
{
    assert(argc == 1);
    PositionalArg arg;
    arg.filePath = argv[0];
    arg.dataSetLabel = g_dataSetLabel;
    g_initialFileNames.push_back(arg);
    // Dataset name must be explicitly specified for each file
    g_dataSetLabel.clear();
    return 0;
}


/// Callback for parsing of multiple hooks
static std::vector<std::string> hookSpec;
static std::vector<std::string> hookPayload;
static int hooks(int argc, const char *argv[])
{
    assert(argc == 3);
    hookSpec.push_back(argv[1]);
    hookPayload.push_back(argv[2]);
    return 0;
}


int main(int argc, char *argv[])
{
    ensureUtf8Argv(&argc, &argv);

    if (argc > 1 && std::string(argv[1]) == "-gui")
    {
        // Detect special command line argument and run the GUI instead of
        // attempting to talk to a remote displaz instance via IPC.
#       ifdef DISPLAZ_CLI_ONLY
        // GUI-less compile - used on windows to create displaz.com
        tfm::format(std::cerr, "ERROR: %s was compiled as without the GUI.", argv[0]);
        return EXIT_FAILURE;
#       else
        return guimain(argc-1, argv+1);
#       endif
    }

    int maxPointCount = -1;
    std::string serverName = "default";
    double posX = -DBL_MAX, posY = -DBL_MAX, posZ = -DBL_MAX;
    double yaw = -DBL_MAX, pitch = -DBL_MAX, roll = -DBL_MAX;
    double rot[9] = {-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX,-DBL_MAX}; // Camera rotation matrix
    double viewRadius = -DBL_MAX;

    std::string shaderName;
    std::string unloadRegex;
    std::string viewLabelName;
    bool noServer = false;

    bool clearFiles = false;
    bool addFiles = false;
    bool mutateData = false;
    std::string annotationText;
    double annotationX = -DBL_MAX;
    double annotationY = -DBL_MAX;
    double annotationZ = -DBL_MAX;
    bool deleteAfterLoad = false;
    bool quitRemote = false;
    bool queryCursor = false;
    bool script = false;

    bool printVersion = false;
    bool printHelp = false;

    std::string hookSpecDef;
    std::string hookPayloadDef;

    std::string notifySpec;
    std::string notifyMessage;

    ArgParse::ArgParse ap;
    ap.options(
        "displaz - A lidar point cloud viewer\n"
        "Usage: displaz [opts] [ [-label label1] file1.las ... ]",
        "%*", storeFileName, "",

        "<SEPARATOR>", "\nData set tagging",
        "-label %s",     &g_dataSetLabel, "Override the displayed dataset label for the next data file on the command line with the given name",

        "<SEPARATOR>", "\nInitial settings / remote commands:",
        "-maxpoints %d", &maxPointCount, "Maximum number of points to load at a time",
        "-noserver",     &noServer,      "Don't attempt to open files in existing window",
        "-server %s",    &serverName,    "Name of displaz instance to message on startup",
        "-shader %s",    &shaderName,    "Name of shader file to load on startup",
        "-viewposition %F %F %F", &posX, &posY, &posZ, "Set absolute view position [X, Y, Z]",
        "-viewradius %F", &viewRadius,   "Set distance to view focus point",
        "-viewlabel %s", &viewLabelName, "Set view on the first index to geometry that matches the given label",
        "-viewangles %F %F %F", &yaw, &pitch, &roll, "Set view angles in degrees [yaw, pitch, roll]. "
                                         "Equivalent to -viewrotation with rotation matrix "
                                         "R_z(roll)*R_x(pitch-90)*R_z(yaw).",
        "-viewrotation %F %F %F %F %F %F %F %F %F", rot+0, rot+1, rot+2, rot+3, rot+4, rot+5, rot+6, rot+7, rot+8,
                                         "Set 3x3 camera rotation matrix. "
                                         "The matrix must be a rotation in row major format; "
                                         "it transforms points into the standard OpenGL camera coordinates "
                                         "(+x right, +y up, -z into the scene). "
                                         "This alternative to -viewangles is supplied to simplify "
                                         "setting camera rotations from a script.",
        "-clear",        &clearFiles,    "Remote: clear all currently loaded files",
        "-unload %s",    &unloadRegex,   "Remote: unload loaded files or annotations who's label matches the given (unix shell style) pattern",
        "-quit",         &quitRemote,    "Remote: close the existing displaz window",
        "-add",          &addFiles,      "Remote: add files to currently open set, instead of replacing those with duplicate labels",
        "-modify",       &mutateData,    "Remote: mutate data already loaded with the matching label (requires displaz .ply with an \"index\" field to indicate mutated points)",
        "-annotation %s %F %F %F", &annotationText, &annotationX, &annotationY, &annotationZ, "Add a text annotation [text, x, y, z]",
        "-rmtemp",       &deleteAfterLoad, "*Delete* files after loading - use with caution to clean up single-use temporary files after loading",
        "-querycursor",  &queryCursor,   "Query 3D cursor location from displaz instance",
        "-script",       &script,        "Script mode: enable several settings which are useful when calling displaz from a script:"
                                         " (a) do not wait for displaz GUI to exit before returning,",
        "-hook %@ %s %s", hooks, &hookSpecDef, &hookPayloadDef, "Hook to listen for specified event [hook_specifier hook_payload]. Payload is cursor or null",
        "-notify %s %s", &notifySpec, &notifyMessage, "Send a GUI notification [spec message] to the user. "
                                         "spec is a specification of how the user will see the message, it must be "
                                         "'log' to add an info message, or 'log:<level>' to add logging message at one of {error,warning,info,debug} levels.",

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

    if (noServer)
    {
        // Use unique random server name if -noserver specified: the GUI still
        // needs arguments passed to it.
        serverName = QUuid::createUuid().toByteArray().constData();
    }

    std::string socketName, lockName;
    getDisplazIpcNames(socketName, lockName, serverName);
    InterProcessLock instanceLock(lockName);
    bool startedGui = false;
    qint64 guiPid = -1;
    if (instanceLock.tryLock())
    {
        // Check that the user hasn't made a mess of the command line options
        if (queryCursor)
        {
            std::cerr << "ERROR: No remote displaz instance found\n";
            return EXIT_FAILURE;
        }
        if (quitRemote || !unloadRegex.empty())
        {
            return EXIT_SUCCESS;
        }

        // Launch the main GUI window in a separate process.
        QString exeName = QCoreApplication::applicationFilePath();
#       ifdef _WIN32
        if (exeName.endsWith(".com"))
            exeName = exeName.replace(exeName.size()-3,3,"exe");
#       endif
        QStringList args;
        args << "-gui"
             << "-instancelock" << QString::fromStdString(lockName)
                                << QString::fromStdString(instanceLock.makeLockId())
             << "-socketname"   << QString::fromStdString(socketName);
        if (!QProcess::startDetached(exeName, args,
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
    std::unique_ptr<IpcChannel> channel = IpcChannel::connectToServer(QString::fromStdString(socketName));
    if (!channel)
    {
        std::cerr << "ERROR: Could not open IPC channel to remote instance - exiting\n";
        return EXIT_FAILURE;
    }

    // Format IPC message, or exit if none specified.
    //
    // TODO: Factor out this socket comms code - sending and recieving of
    // messages should happen in a centralised place.
    if (clearFiles)
    {
        channel->sendMessage("CLEAR_FILES");
    }
    if (!g_initialFileNames.empty())
    {
        QByteArray command;
        QDir currentDir = QDir::current();
        command = "OPEN_FILES\n";
        if (mutateData)
        {
            command += QByteArray("MUTATE_EXISTING");
            command += '\0';
        }
        if (!addFiles)
        {
            command += QByteArray("REPLACE_LABEL");
            command += '\0';
        }
        if (deleteAfterLoad)
        {
            command += QByteArray("DELETE_AFTER_LOAD");
            command += '\0';
        }
        for (size_t i = 0; i < g_initialFileNames.size(); ++i)
        {
            const PositionalArg& arg = g_initialFileNames[i];
            command += '\n';
            command += currentDir.absoluteFilePath(QString::fromStdString(arg.filePath)).toUtf8();
            command += '\0';
            command += QByteArray(arg.dataSetLabel.data(), (int)arg.dataSetLabel.size());
        }
        channel->sendMessage(command);
    }
    if (annotationText != "" &&
        annotationX != -DBL_MAX &&
        annotationY != -DBL_MAX &&
        annotationZ != -DBL_MAX)
    {
        channel->sendMessage(QByteArray("ANNOTATE\n") +
                             g_dataSetLabel.c_str() + "\n" +
                             annotationText.c_str() + "\n" +
                             QByteArray().setNum(annotationX, 'e', 17) + "\n" +
                             QByteArray().setNum(annotationY, 'e', 17) + "\n" +
                             QByteArray().setNum(annotationZ, 'e', 17));
    }
    if (!unloadRegex.empty())
    {
        channel->sendMessage(QByteArray("UNLOAD_FILES\n") + unloadRegex.c_str());
    }
    if (!viewLabelName.empty())
    {
        channel->sendMessage(QByteArray("SET_VIEW_LABEL\n") + viewLabelName.c_str());
    }
    if (posX != -DBL_MAX)
    {
        channel->sendMessage("SET_VIEW_POSITION\n" +
                             QByteArray().setNum(posX, 'e', 17) + "\n" +
                             QByteArray().setNum(posY, 'e', 17) + "\n" +
                             QByteArray().setNum(posZ, 'e', 17));
    }
    if (yaw != -DBL_MAX)
    {
        channel->sendMessage("SET_VIEW_ANGLES\n" +
                             QByteArray().setNum(yaw)   + "\n" +
                             QByteArray().setNum(pitch) + "\n" +
                             QByteArray().setNum(roll));
    }
    if (rot[0] != -DBL_MAX)
    {
        QByteArray command("SET_VIEW_ROTATION\n");
        for(int i = 0; i < 9; ++i)
        {
            command += QByteArray().setNum(rot[i]);
            if (i != 8)
                command += "\n";
        }
        channel->sendMessage(command);
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
        try
        {
            channel->sendMessage("QUERY_CURSOR");
            QByteArray msg = channel->receiveMessage();
            std::cout.write(msg.data(), msg.length());
            std::cout << "\n";
        }
        catch (DisplazError & e)
        {
            std::cerr << "ERROR: QUERY_CURSOR message failed:\n" << e.what();
            return EXIT_FAILURE;
        }
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
    if (!notifySpec.empty())
    {
        channel->sendMessage(QByteArray("NOTIFY\n") +
                             notifySpec.c_str() + "\n" +
                             notifyMessage.c_str());
    }
    if(!hookPayload.empty())
    {
        QByteArray msg;
        QByteArray message = QByteArray("HOOK");
        for(size_t i=0; i<hookPayload.size(); i++)
        {
            message = message + QByteArray("\n")
                              + QByteArray(hookSpec[i].data(), (int)hookSpec[i].size()) + QByteArray("\n")
                              + QByteArray(hookPayload[i].data(), (int)hookPayload[i].size());
        }

        try
        {
            channel->sendMessage(message);
            do
            {
                msg = channel->receiveMessage(-1);
                std::cout.write(msg.data(), msg.length());
                std::cout.flush();
            }
            while(true);
        }
        catch (DisplazError & e)
        {
            std::cerr << "ERROR: Connection to GUI broken: " << e.what();
            return EXIT_FAILURE;
        }
    }

    if (startedGui && !script)
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
