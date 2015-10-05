// As shown here: https://github.com/mortennobel/QtOpenGL3.2Core

#include <QGLContext>

#if QT_VERSION < 0x050000 // if less than 5.0.0
void* select_3_2_mac_visual(GDHandle handle, int depthBufferSize)
{
    static const int Max = 40;
    NSOpenGLPixelFormatAttribute attribs[Max];
    int cnt = 0;

    attribs[cnt++] = NSOpenGLPFAOpenGLProfile;
    attribs[cnt++] = NSOpenGLProfileVersion3_2Core;

    attribs[cnt++] = NSOpenGLPFADoubleBuffer;

    attribs[cnt++] = NSOpenGLPFADepthSize;
    //attribs[cnt++] = (NSOpenGLPixelFormatAttribute)(depthBufferSize==-1)?32:depthBufferSize;
    attribs[cnt++] = (NSOpenGLPixelFormatAttribute)32;

    // attribs[cnt++] = NSOpenGLPFAAllowOfflineRenderers;
    attribs[cnt++] = NSOpenGLPFANoRecovery;
    attribs[cnt++] = NSOpenGLPFAAccelerated;

    attribs[cnt++] = NSOpenGLPFAColorSize;
    attribs[cnt++] = (NSOpenGLPixelFormatAttribute)24;

    attribs[cnt++] = NSOpenGLPFAAlphaSize;
    attribs[cnt++] = (NSOpenGLPixelFormatAttribute)8;

    attribs[cnt++] = NSOpenGLPFASupersample;

    attribs[cnt++] = NSOpenGLPFASampleBuffers;
    attribs[cnt++] = (NSOpenGLPixelFormatAttribute)1;

    // attribs[cnt++] = NSOpenGLPFASamples;
    // attribs[cnt++] = (NSOpenGLPixelFormatAttribute)4;

    attribs[cnt++] = NSOpenGLPFAMultisample;

    attribs[cnt] = (NSOpenGLPixelFormatAttribute)0;
    Q_ASSERT(cnt < Max);

    return [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
}
#endif
