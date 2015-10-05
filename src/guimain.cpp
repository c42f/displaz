// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "mainwindow.h"

//#include <QtCore/QDataStream>
#include <QtCore/QTextCodec>
#include <QtGui/QApplication>
#include <QtOpenGL/QGLFormat>

#include "argparse.h"
#include "config.h"
#include "InterProcessLock.h"

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


/// Run the main GUI window
int main(int argc, char* argv[])
{
    std::string lockName;
    std::string lockId;
    std::string socketName;

    ArgParse::ArgParse ap;
    ap.options(
        "displaz-gui - don't use this directly, use the displaz commandline helper instead)",
        "-instancelock %s %s", &lockName, &lockId, "Single instance lock name and ID to reacquire",
        "-socketname %s", &socketName,             "Local socket name for IPC",
        NULL
    );

    if(ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        std::cerr << "ERROR: " << ap.geterror() << std::endl;
        return EXIT_FAILURE;
    }

    QApplication app(argc, argv);
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));

    setupQFileSearchPaths();

    Q_INIT_RESOURCE(resource);

    qRegisterMetaType<std::shared_ptr<Geometry>>("std::shared_ptr<Geometry>");

    // Multisampled antialiasing - this makes rendered point clouds look much
    // nicer, but also makes the render much slower, especially on lower
    // powered graphics cards.
    QGLFormat f = QGLFormat::defaultFormat();
    f.setVersion( 3, 2 );
    f.setProfile( QGLFormat::CoreProfile ); // Requires >=Qt-4.8.0
    //f.setSampleBuffers(true);
    QGLFormat::setDefaultFormat(f);

    PointViewerMainWindow window(f);
    InterProcessLock instanceLock(lockName);
    if (!lockId.empty())
        instanceLock.inherit(lockId);
    if (!socketName.empty())
        window.startIpcServer(QString::fromStdString(socketName));
    window.show();
    return app.exec();
}

