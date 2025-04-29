// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#pragma once

#include <GL/glew.h>

#include "util.h"

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

