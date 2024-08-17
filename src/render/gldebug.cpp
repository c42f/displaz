// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "gldebug.h"

#include <map>

#include "tinyformat.h"

void _glError(const char *file, int line)
{
    GLenum err = glGetError();

    while (err != GL_NO_ERROR)
    {
        std::string error;

        switch (err)
        {
            case GL_INVALID_OPERATION:      error="INVALID_OPERATION";      break;
            case GL_INVALID_ENUM:           error="INVALID_ENUM";           break;
            case GL_INVALID_VALUE:          error="INVALID_VALUE";          break;
            case GL_OUT_OF_MEMORY:          error="OUT_OF_MEMORY";          break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:  error="INVALID_FRAMEBUFFER_OPERATION";  break;
        }

        tfm::printfln("GL_%s - %s:%i", error, file, line);
        err = glGetError();
    }
}

void _glFrameBufferStatus(GLenum target, const char *file, int line)
{
    GLenum fbStatus = glCheckFramebufferStatus(target);

    if (fbStatus == GL_FRAMEBUFFER_COMPLETE)
        return;

    std::string status;

    switch(fbStatus) {
        case GL_INVALID_ENUM:           status="?? (bad target)"; break;
        //case GL_FRAMEBUFFER_COMPLETE:   status="COMPLETE";      break;
        case GL_FRAMEBUFFER_UNDEFINED:  status="UNDEFINED";     break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: status="INCOMPLETE_ATTACHMENT";        break;
        //case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: status="INCOMPLETE_MISSING_ATTACHMENT";      break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: status="INCOMPLETE_DRAW_BUFFER";      break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: status="INCOMPLETE_READ_BUFFER";      break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: status="INCOMPLETE_MULTISAMPLE";      break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: status="INCOMPLETE_LAYER_TARGETS";  break;
        //case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: status="INCOMPLETE_DIMENSIONS";      break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: status="INCOMPLETE_MISSING_ATTACHMENT";      break;
        case GL_FRAMEBUFFER_UNSUPPORTED: status="UNSUPPORTED";      break;
        default:                        status="???";    break;
    }

    tfm::printfln("GL_FRAMEBUFFER_%s (%d) - %s:%i", status, fbStatus, file, line);
}

void _debugMessageCallback(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *msg, const void *data)
{
    const std::map<GLenum, const char*> sourceMap =
        {
            { GL_DEBUG_SOURCE_API,             "API"             },
            { GL_DEBUG_SOURCE_WINDOW_SYSTEM,   "WINDOW SYSTEM"   },
            { GL_DEBUG_SOURCE_SHADER_COMPILER, "SHADER COMPILER" },
            { GL_DEBUG_SOURCE_THIRD_PARTY,     "THIRD PARTY"     },
            { GL_DEBUG_SOURCE_APPLICATION,     "APPLICATION"     }
        };

    const auto sourceIterator = sourceMap.find(source);
    const char* _source = sourceIterator != sourceMap.end() ? sourceIterator->second : "UNKNOWN";

    const std::map<GLenum, const char*> typeMap =
        {
            { GL_DEBUG_TYPE_ERROR,               "ERROR"               },
            { GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "DEPRECATED BEHAVIOR" },
            { GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR,  "UDEFINED BEHAVIOR"   },
            { GL_DEBUG_TYPE_PORTABILITY,         "PORTABILITY"         },
            { GL_DEBUG_TYPE_PERFORMANCE,         "PERFORMANCE"         },
            { GL_DEBUG_TYPE_OTHER,               "OTHER"               },
            { GL_DEBUG_TYPE_MARKER,              "MARKER"              },
        };

    const auto typeIterator = typeMap.find(type);
    const char* _type = typeIterator != typeMap.end() ? typeIterator->second : "UNKNOWN";

    const std::map<GLenum, const char*> severityMap =
        {
            { GL_DEBUG_SEVERITY_HIGH,         "HIGH"         },
            { GL_DEBUG_SEVERITY_MEDIUM,       "MEDIUM"       },
            { GL_DEBUG_SEVERITY_LOW,          "LOW"          },
            { GL_DEBUG_SEVERITY_NOTIFICATION, "NOTIFICATION" },
        };

    const auto severityIterator = severityMap.find(severity);
    const char* _severity = severityIterator != severityMap.end() ? severityIterator->second : "UNKNOWN";

    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }

    tfm::printfln("%d: %s of %s severity, raised from %s: %s",
            id, _type, _severity, _source, msg);
}
