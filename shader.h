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

#include <memory>

#include <QtOpenGL/QGLShader>
#include <QtOpenGL/QGLShaderProgram>


/// Representation of a shader "parameter" (uniform variable or attribute)
struct ShaderParam
{
    enum Type {
        Float,
        Int,
        Vec3
    };

    Type type;          ///< Variable type
    QByteArray name;    ///< Name of the variable in the shader
    QString uiName;     ///< Name for display in parameter UI

    ShaderParam(Type type=Float, QByteArray name="", QString uiName="")
        : type(type), name(name), uiName(uiName) {}
};


/// Wrapper for QGLShader, with functionality added to parse
/// the list of parameters.
class Shader
{
    public:
        Shader(QGLShader::ShaderType type, const QGLContext* context)
            : m_shader(type, context)
        { }

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
        QGLShader* shader()
        {
            return &m_shader;
        }

        bool compileSourceCode(const QByteArray& src);

    private:
        QList<ShaderParam> m_uniforms;
        QGLShader m_shader;  ///< Underlying shader
        QByteArray m_source; ///< Non-mangled source code
};


/// Wrapper around QGLShaderProgram
class ShaderProgram : public QObject
{
    Q_OBJECT

    public:
        ShaderProgram(const QGLContext * context, QObject* parent = 0);

        QGLShaderProgram& shaderProgram() { return *m_shaderProgram; }

        void setupParameterUI(QWidget* parentWidget);
        void setUniforms();

        QByteArray vertexShader() const;
        QByteArray fragmentShader() const;

    public slots:
        void setVertexShader(QString src);
        void setFragmentShader(QString src);

        void setPointSize(double size);
        void setExposure(double intensity);
        void setContrast(double power);
        void setSelector(int sel);

    signals:
        void paramsChanged();
        void uniformValuesChanged();

    private:
        void setupParameters();

    private:
        const QGLContext* m_context;
        double m_pointSize;
        double m_exposure;
        double m_contrast;
        int m_selector;
        std::unique_ptr<Shader> m_vertexShader;
        std::unique_ptr<Shader> m_fragmentShader;
        std::unique_ptr<QGLShaderProgram> m_shaderProgram;
};


/*
//------------------------------------------------------------------------------
/// Holder for a shader parameter value and associated metadata
class ShaderParamValue : public QObject
{
    Q_OBJECT

    public:
        ShaderParamValue(const ShaderParam& param) : m_param(param) { }

        QVariant value() const { return m_value; }

        const ShaderParam& paramDesc() const { return m_paramDesc; }

    signals:
        void valueChanged();

    public slots:
        void setValue(QVariant value) { return m_value; }

    private:
        ShaderParam m_param;
        QVariant m_value;
};
*/


