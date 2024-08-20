// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_ANNOTATION_H_INCLUDED
#define DISPLAZ_ANNOTATION_H_INCLUDED

#include <memory>

#include "glutil.h"
#include <QString>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

struct TransformState;

/// Annotations display text, always face the camera, and aren't obscured by
/// other objects
class Annotation
{

    public:
        Annotation(const QString& label,
                   GLuint annotationShaderProg,
                   const QString& text,
                   Imath::V3d position);

        ~Annotation();

        QString label() const { return m_label; }

        /// Draw the annotation using the given shader program
        ///
        /// Requires that `annotationShaderProg` is already bound and that
        /// the viewportSize uniform variable has been set.
        ///
        /// transState specifies the camera transform.
        void draw(QOpenGLShaderProgram& annotationShaderProg,
                  const TransformState& transState) const;

    private:
        QString m_label;
        QString m_text;
        Imath::V3d m_position;
        std::shared_ptr<QOpenGLTexture> m_texture;
        GLuint m_vao;
};

#endif // DISPLAZ_ANNOTATION_H_INCLUDED
