// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#ifndef GLUTIL_H_INCLUDED
#define GLUTIL_H_INCLUDED

//#include <GL/glew.h>
#include <QOpenGLFunctions_3_2_Core>

#include <vector>

#include "util.h"
#include "typespec.h"

//------------------------------------------------------------------------------
/// Utility to handle transformation state
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

    /// Load matrix uniforms onto the currently bound shader program:
    ///
    ///   "projectionMatrix"
    ///   "modelViewMatrix"
    ///   "modelViewProjectionMatrix"
    ///
    void setUniforms(GLuint prog) const;

    /// Load matrices into traditional openGL transform stack
    ///
    /// Should be removed eventually!
    void load() const;
};


//----------------------------------------------------------------------
/// Utilites for drawing simple primitives
void drawBoundingBox(const TransformState& transState,
                     const Imath::Box3f& bbox, const Imath::C3f& col);
void drawBoundingBox(const TransformState& transState,
                     const Imath::Box3d& bbox, const Imath::C3f& col);

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
