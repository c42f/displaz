// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_BILLBOARD_H_INCLUDED
#define DISPLAZ_BILLBOARD_H_INCLUDED

#include <memory>

#include "glutil.h"
#include <QString>
#include <QGLShaderProgram>

struct TransformState;

/// Billboards display text, always face the camera, and aren't obscured by
/// other objects
class Billboard
{

    public:
        Billboard(QGLShaderProgram& billboardShaderProg,
                  const QString& text,
                  Imath::V3d position);

        ~Billboard();

        /// Draw the billboard using the given shader program
        ///
        /// Requires that `billboardShaderProg` is already bound and that
        /// the viewportSize uniform variable has been set.
        ///
        /// transState specifies the camrea transform.
        void draw(QGLShaderProgram& billboardShaderProg,
                  const TransformState& transState) const;

    private:
        QString m_text;
        Imath::V3d m_position;
        std::shared_ptr<Texture> m_texture;
        GLuint m_vao;
};

#endif // DISPLAZ_BILLBOARD_H_INCLUDED
