// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef SHADER_H_INCLUDED
#define SHADER_H_INCLUDED

#include <QList>
#include <QByteArray>
#include <QOpenGLShader>

#include "ShaderParam.h"

/// Wrapper for QOpenGLShader, with functionality added to parse
/// the list of uniform parameters.
class Shader
{
    public:
        Shader(QOpenGLShader::ShaderType type, const char* label = nullptr);

        /// Return list of uniform shader parameters
        const QList<ShaderParam>& uniforms() const
        {
            return m_uniforms;
        }

        /// Return original source code.
        QByteArray sourceCode() const
        {
            return m_source;
        }

        /// Access to underlying shader
        QOpenGLShader* shader()
        {
            return &m_shader;
        }

        bool compileSourceCode(const QByteArray& src);

    private:
        QList<ShaderParam> m_uniforms;
        QOpenGLShader m_shader;  ///< Underlying shader
        QByteArray m_source; ///< Non-mangled source code
};


#endif // SHADER_H_INCLUDED
