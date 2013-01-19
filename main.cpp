#include "mainwindow.h"
#include "ptview.h"

#include <QtGui/QApplication>
#include <QtOpenGL/QGLFormat>

#include "argparse.h"


QStringList g_pointFileNames;
static int storeFileName (int argc, const char *argv[])
{
    for(int i = 0; i < argc; ++i)
        g_pointFileNames.push_back (argv[i]);
    return 0;
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ArgParse::ArgParse ap;
    int maxPointCount = 10000000;

    ap.options(
        "qtlasview - view a LAS point cloud\n"
        "Usage: qtlasview [opts] file1.las [file2.las ...]",
        "%*", storeFileName, "",
        "-maxpoints %d", &maxPointCount, "Maximum number of points to load at a time",
        NULL
    );

    if(ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        std::cerr << ap.geterror() << std::endl;
        ap.usage();
        return EXIT_FAILURE;
    }

    // Turn on multisampled antialiasing - this makes rendered point clouds
    // look much nicer.
    QGLFormat f = QGLFormat::defaultFormat();
    //f.setSampleBuffers(true);
    QGLFormat::setDefaultFormat(f);

    PointViewerMainWindow window;
    window.captureStdout();
    window.pointView().setMaxPointCount(maxPointCount);
    if(!g_pointFileNames.empty())
        window.pointView().loadPointFiles(g_pointFileNames);
    window.show();

    return app.exec();
}

