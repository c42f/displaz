// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef GLDEBUG_H_INCLUDED
#define GLDEBUG_H_INCLUDED

#include <GL/glew.h>

void _glError(const char *file, int line);
void _glFrameBufferStatus(GLenum target, const char *file, int line);


// #define GL_CHECK
#ifdef GL_CHECK

    #define glCheckError() _glError(__FILE__,__LINE__)
    #define glCheckFrameBufferStatus() _glFrameBufferStatus(GL_FRAMEBUFFER, __FILE__,__LINE__)

#else

    #define glCheckError()
    #define glCheckFrameBufferStatus()

#endif //GL_CHECK

void _debugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *msg, const void *data);

class PushDebugGroup
{
public:
    PushDebugGroup(const char* message)
    {
        #ifdef GL_CHECK
        if (glPushDebugGroup)
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
        }
        #endif
    }

    ~PushDebugGroup()
    {
        #ifdef GL_CHECK
        if (glPopDebugGroup)
        {
            glPopDebugGroup();
        }
        #endif
    }
};

inline void pushDebugGroup(const char* message)
{
    #ifdef GL_CHECK
    if (glPushDebugGroup)
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, message);
    }
    #endif
}

inline void popDebugGroup()
{
    #ifdef GL_CHECK
    if (glPopDebugGroup)
    {
        glPopDebugGroup();
    }
    #endif
}

inline void objectLabel(GLenum identifier, GLuint name, const char* label)
{
    #ifdef GL_CHECK
    if (glObjectLabel)
    {
        glObjectLabel(identifier, name, -1, label);
    }
    #endif
}

#endif // GLDEBUG_H_INCLUDED
