// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "TransformState.h"

#include "glutil.h"

void TransformState::setUniform(GLuint prog, const char* name, const M44d& mat)
{
    GLint loc = glGetUniformLocation(prog, name);
    if (loc == -1)
        return;
    M44f M(mat);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &M[0][0]);
}

TransformState TransformState::translate(const Imath::V3d& offset) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().setTranslation(offset) * modelViewMatrix;
    return res;
}

TransformState TransformState::scale(const Imath::V3d& scalar) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().setScale(scalar) * modelViewMatrix;
    return res;
}

TransformState TransformState::rotate(const Imath::V4d& rotation) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().rotate(V3d(rotation.x*rotation.w,rotation.y*rotation.w,rotation.z*rotation.w)) * modelViewMatrix;
    return res;
}


void TransformState::setUniforms(GLuint prog) const
{
    // Note: The matrices must have a sensible representation in float32
    // precision by the time they get here.
    setUniform(prog, "projectionMatrix", projMatrix);
    setUniform(prog, "modelViewMatrix", modelViewMatrix);
    M44d mvproj = modelViewMatrix * projMatrix;
    setUniform(prog, "modelViewProjectionMatrix", mvproj);
}

void TransformState::setOrthoProjection(double left, double right, double bottom, double top, double nearVal, double farVal)
{
    // Note: Create an orthographic matrix from input values
    if ((left == right) || (bottom == top) || (nearVal == farVal))
        throw DisplazError("Invalid input for orthographic projection.");

    double xx = 2.0/(right-left);
    double yy = 2.0/(top-bottom);
    double zz = -2.0/(farVal-nearVal);

    double tx = -(right+left)/(right-left);
    double ty = -(top+bottom)/(top-bottom);
    double tz = -(farVal+nearVal)/(farVal-nearVal);

    projMatrix = M44d(xx,0.0,0.0,0.0,
                      0.0,yy,0.0,0.0,
                      0.0,0.0,zz,0.0,
                      tx,ty,tz,1.0);
}

