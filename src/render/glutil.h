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

#include <vector>
#include <cassert>

#include "util.h"
#include "typespec.h"

struct TransformState
{
    Imath::V2i viewSize;
    Imath::M44d projMatrix;
    Imath::M44d modelViewMatrix;

    TransformState(const Imath::V2i& viewSize, const M44d& projMatrix,
                   const M44d& modelViewMatrix)
        : viewSize(viewSize), projMatrix(projMatrix),
        modelViewMatrix(modelViewMatrix)
    { }

    /// Return position of camera in model space
    V3d cameraPos() const
    {
        return V3d(0)*modelViewMatrix.inverse();
    }

    /// Translate model by given offset
    TransformState translate(const Imath::V3d& offset) const;

    /// Scale model by given scalar
    TransformState scale(const Imath::V3d& scalar) const;

    /// Rotate model by given rotation vector
    TransformState rotate(const Imath::V4d& rotation) const;

    /// Load matrix uniforms onto the currently bound shader program:
    ///
    ///   "projectionMatrix"
    ///   "modelViewMatrix"
    ///   "modelViewProjectionMatrix"
    ///
    static void setUniform(GLuint prog, const char* name, const M44d& mat);
    void setUniforms(GLuint prog) const;
    void setProjUniform(GLuint prog) const;

    void setOrthoProjection(double left, double right, double bottom, double top, double nearVal, double farVal);
};


//----------------------------------------------------------------------
/// Utilites for drawing simple primitives
void drawBox(const TransformState& transState,
             const Imath::Box3d& bbox, const Imath::C3f& col, const GLuint& shaderProgram);
void drawBox(const TransformState& transState,
             const Imath::Box3f& bbox, const Imath::C3f& col, const GLuint& shaderProgram);
void drawBoundingBox(const TransformState& transState, const GLuint& bboxVertexArray,
                     const Imath::V3f& offset, const Imath::C3f& col, const GLuint& shaderProgram);

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
// Texture utility

struct Texture
{
    Texture(const QImage& i)
    :   image(i),
        target(GL_TEXTURE_2D),
        texture(0)
    {
    }

    ~Texture()
    {
        if (texture)
        {
            glDeleteTextures(1, &texture);
        }
    }

    void bind(int sampler) const
    {
        if (!texture)
        {
            glGenTextures(1, &texture);
            glBindTexture(target, texture);
            // TODO better handling for non-RGBA formats
            assert(image.format()==QImage::Format_ARGB32);
            if (image.format()==QImage::Format_ARGB32)
                glTexImage2D(target, 0, GL_RGBA, image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());       
            glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        else
        {
            // TODO: this has to become more sophisticated, if we ever want to have more than one texture bound
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(target, texture);
            if (sampler >= 0)
            {
                glBindSampler(texture, sampler);
            }
        }
    }

    void bind() const
    {
        bind(-1);
    }

            QImage image;
            GLint  target;
    mutable GLuint texture;
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


void _glError(const char *file, int line);
void _glFrameBufferStatus(GLenum target, const char *file, int line);


// #define GL_CHECK
#ifdef GL_CHECK

    #define glCheckError() _glError(__FILE__,__LINE__)
    #define glFrameBufferStatus(TARGET) _glFrameBufferStatus(TARGET, __FILE__,__LINE__)

#else

    #define glCheckError()
    #define glFrameBufferStatus(TARGET)

#endif //GL_CHECK

#endif // GLUTIL_H_INCLUDED
