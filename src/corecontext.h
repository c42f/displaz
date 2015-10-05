// As shown here: https://github.com/mortennobel/QtOpenGL3.2Core

#ifndef DISPLAZ_CORECONTEXT_H
#define DISPLAZ_CORECONTEXT_H

#include <QtOpenGL/QGLContext>
#include <QtOpenGL/QGLFormat>

#if defined(Q_OS_MAC)
#if QT_VERSION < 0x050000 && QT_VERSION >= 0x040800 // if less than Qt 5.0.0
void* select_3_2_mac_visual(GDHandle handle, int depthBufferSize);
#endif
#endif

struct CoreContext : public QGLContext {
    CoreContext(const QGLFormat& format, QPaintDevice* device) : QGLContext(format,device) {}
    CoreContext(const QGLFormat& format) : QGLContext(format) {}

#if defined(Q_OS_MAC)
#if QT_VERSION < 0x050000 && QT_VERSION >= 0x040800 // if less than Qt 5.0.0
    virtual void* chooseMacVisual(GDHandle handle) {
        return select_3_2_mac_visual(handle, this->format().depthBufferSize());
    }
#endif
#endif
};

#endif //DISPLAZ_CORECONTEXT_H
