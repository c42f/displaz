// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "PointViewerMainWindow.h"

//#include <QDataStream>
#include <QTextCodec>
#include <QApplication>
#include <QGLFormat>

#include "argparse.h"
#include "config.h"
#include "InterProcessLock.h"
#include "util.h"

class Geometry;
class GeometryMutator;


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
    std::string serverName;

    ArgParse::ArgParse ap;
    ap.options(
        "displaz-gui - don't use this directly, use the displaz commandline helper instead)",
        "-instancelock %s %s", &lockName, &lockId, "Single instance lock name and ID to reacquire",
        "-socketname %s",      &socketName,        "Local socket name for IPC",
        "-server %s",          &serverName,        "DEBUG: Compute lock file and socket name; do not inherit lock",
        NULL
    );

    if(ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        std::cerr << "ERROR: " << ap.geterror() << std::endl;
        return EXIT_FAILURE;
    }

    QApplication app(argc, argv);
#ifdef DISPLAZ_USE_QT4
    // Qt5: no longer required
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("utf8"));
#endif

    setupQFileSearchPaths();

    Q_INIT_RESOURCE(resource);

    qRegisterMetaType<std::shared_ptr<Geometry>>("std::shared_ptr<Geometry>");
    qRegisterMetaType<std::shared_ptr<GeometryMutator>>("std::shared_ptr<GeometryMutator>");

    // Multisampled antialiasing - this makes rendered point clouds look much
    // nicer, but also makes the render much slower, especially on lower
    // powered graphics cards.
    QGLFormat f = QGLFormat::defaultFormat();
    f.setVersion( 3, 2 );
    f.setProfile( QGLFormat::CoreProfile ); // Requires >=Qt-4.8.0
    //f.setSampleBuffers(true);
    QGLFormat::setDefaultFormat(f);

    PointViewerMainWindow window(f);

    // Inherit instance lock (or debug: acquire it)
    if (!serverName.empty())
        getDisplazIpcNames(socketName, lockName, serverName);
    InterProcessLock instanceLock(lockName);
    if (!lockId.empty())
    {
        instanceLock.inherit(lockId);
    }
    else if (!serverName.empty())
    {
        if (!instanceLock.tryLock())
        {
            std::cerr << "ERROR: Another displaz instance has the lock\n";
            return EXIT_FAILURE;
        }
    }

    if (!socketName.empty())
        window.startIpcServer(QString::fromStdString(socketName));
    window.show();
    return app.exec();
}
