// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef GLUTIL_H_INCLUDED
#define GLUTIL_H_INCLUDED


#include <GL/glew.h>

#ifdef __APPLE__
    #include <OpenGL/glext.h>
#else
    #include <GL/gl.h>
#endif

#define QT_NO_OPENGL_ES_2

#include <QImage>
#include <QGLWidget>

#include <vector>
#include <cassert>

#include "util.h"
#include "typespec.h"
#include "TransformState.h"

//----------------------------------------------------------------------
/// Utilites for drawing simple primitives
void drawBox(const TransformState& transState,
             const Imath::Box3d& bbox, const Imath::C3f& col, const GLuint& shaderProgram);
void drawBox(const TransformState& transState,
             const Imath::Box3f& bbox, const Imath::C3f& col, const GLuint& shaderProgram);
void drawBoundingBox(const TransformState& transState, const GLuint& bboxVertexArray,
                     const Imath::V3d& offset, const Imath::C3f& col, const GLuint& shaderProgram);

/// Draw a sphere using the given shader.  May be semitransparent.
///
/// The sphere is drawn using a polygonal approximation generated from the
/// usual spherical coordintes parameterization.  In order for transparency to
/// work correctly, the parameterization is evaluated in camera space, which
/// means the model view matrix must be a rigid body transformation.
///
/// transState - current transformations (modelview must be rigid body trans!)
/// centre - sphere centre
/// radius - sphere radius
/// shaderProgram - Compiled OpenGL shader program id
/// color - color for sphere.  May be semitransparent
/// nphi,ntheta - number of steps in spherical coordinate parameterization
void drawSphere(const TransformState& transState,
                const Imath::V3d& centre, double radius,
                GLuint shaderProgram, const Imath::C4f& color,
                int nphi = 50, int ntheta = 10);

//----------------------------------------------------------------------
// Utilities for OpenEXR / OpenGL interoperability.
//
// Technically we could use the stuff from ImathGL instead here, but it has
// portability problems for OSX due to how it includes gl.h (this is an
// libilmbase bug, at least up until 1.0.2)
inline void glTranslate(const Imath::V3f& v)
{
    glTranslatef(v.x, v.y, v.z);
}

inline void glVertex(const Imath::V3f& v)
{
    glVertex3f(v.x, v.y, v.z);
}

inline void glVertex(const Imath::V2f& v)
{
    glVertex2f(v.x, v.y);
}

inline void glColor(const Imath::C3f& c)
{
    glColor3f(c.x, c.y, c.z);
}

inline void glLoadMatrix(const Imath::M44f& m)
{
    glLoadMatrixf((GLfloat*)m[0]);
}


//------------------------------------------------------------------------------
/// OpenGL buffer lifetime management.
///
/// Use this to allocate a temporary buffer for passing data to the GPU.  For
/// instance, for associating vertex buffer data with a vertex array object:
///
/// ```
/// {
///     GLuint vertexArray;
///     glGenVertexArrays(1, &vertexArray);
///     glBindVertexArray(vertexArray);
///
///     // Associate some array data with the VBO
///     GlBuffer positionBuffer;
///     positionBuffer.bind(GL_ARRAY_BUFFER)
///     glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), &m_verts[0], GL_STATIC_DRAW);
///
///     // Critical: unbind the VBO to ensure that OpenGL state doesn't leak
///     // out of the function
///     glBindVertexArray(0);
/// }
/// ```
class GlBuffer
{
public:
    GlBuffer() : m_id(0) {}

    GlBuffer(const GlBuffer&) = delete;

    GlBuffer(GlBuffer&& rhs)
    {
        m_id = rhs.m_id;
        rhs.m_id = 0;
    }

    bool init()
    {
        if (m_id)
            glDeleteBuffers(1, &m_id);
        m_id = 0;
        glGenBuffers(1, &m_id);
        return m_id != 0;
    }

    void bind(GLenum target)
    {
        if (m_id || init())
            glBindBuffer(target, m_id);
    }

    ~GlBuffer()
    {
        if (m_id)
            glDeleteBuffers(1, &m_id);
    }

private:
    GLuint m_id;
};


//------------------------------------------------------------------------------
/// Framebuffer resource wrapper with color and depth attachements for the
/// particular framebuffer settings needed in the incremental framebuffer in
/// View3D.cpp
class Framebuffer
{
    public:
        /// Generate an uninitialized framebuffer.
        Framebuffer() = default;
        ~Framebuffer()
        {
            destroy();
        }

        /// Initialize framebuffer with given width and height.
        void init(int width, int height);

        void destroy()
        {
            if (m_fbo != 0)
                glDeleteFramebuffers(1, &m_fbo);
            if (m_colorBuf != 0)
                glDeleteRenderbuffers(1, &m_colorBuf);
            if (m_zBuf != 0)
                glDeleteRenderbuffers(1, &m_zBuf);
            m_fbo = 0;
            m_colorBuf = 0;
            m_zBuf = 0;
        }

        /// Get OpenGL identifier for the buffer
        GLuint id() const
        {
            assert(m_fbo != 0);
            return m_fbo;
        }

    private:
        GLuint m_fbo = 0;
        GLuint m_colorBuf = 0;
        GLuint m_zBuf = 0;
};



//------------------------------------------------------------------------------
// Shader utilities

/// Metadata for an OpenGL shader input attribute
///
/// A compiled shader can be queried for metadata about shader attributes.
/// This allows us to do various things, such as set sensible default values
/// when points don't have a desired attribute.
struct ShaderAttribute
{
    std::string name; /// Name of attribute
    int type;     /// Attribute type as returned by glGetActiveAttrib()
    int count;    /// Number of elements in array
    int rows;     /// Number of rows for vectors/matrices
    int cols;     /// Number of columns for matrices (1 otherwise)
    int location; /// Location for use with glVertexAttribPointer
    TypeSpec::Type baseType; /// Associated displaz base type
};


/// Get list of all active attributes on the given shader program
std::vector<ShaderAttribute> activeShaderAttributes(GLuint prog);


/// Debug: print full list of active shader attributes for the given shader program
void printActiveShaderAttributes(GLuint prog);


const ShaderAttribute* findAttr(const std::string& name,
                                const std::vector<ShaderAttribute>& attrs);


#endif // GLUTIL_H_INCLUDED
