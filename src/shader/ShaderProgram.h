// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef SHADER_PROGRAM_H_INCLUDED
#define SHADER_PROGRAM_H_INCLUDED

#include <memory>

#include <QObject>
#include <QByteArray>
#include <QGLShaderProgram>

#include "Shader.h"
#include "ShaderParam.h"

/// Wrapper around QGLShaderProgram providing parameter tweaking UI
///
/// When compiling a new shader, the shader source code is scanned for
/// annotations in the comments which indicate which uniform values should be
/// tweakable by the user.  An appropriate UI editor widget is automatically
/// created for each such uniform value by a call to setupParameterUI().
///
class ShaderProgram : public QObject
{
    Q_OBJECT

    public:
        ShaderProgram(QObject* parent = 0);

        /// Access to the underlying shader program
        QGLShaderProgram& shaderProgram() { return *m_shaderProgram; }

        /// Set up UI for the shader
        void setupParameterUI(QWidget* parentWidget);
        /// Send current uniform values to the underlying OpenGL shader
        void setUniforms();

        /// Read shader source from given file and call setShader()
        bool setShaderFromSourceFile(QString fileName);

        /// Get shader source code
        QByteArray shaderSource() const;

        /// Return true if shader program is ready to use
        bool isValid() const { return static_cast<bool>(m_shaderProgram); }

    public slots:
        /// Set, compile and link shader source.
        ///
        /// Retain old shader if compilation or linking fails
        ///
        /// The shader source should contain both vertex and fragment shaders,
        /// separated inside #ifdef blocks using the macros VERTEX_SHADER and
        /// FRAGMENT_SHADER, which will be defined as appropriate when
        /// compiling the individual shader types.
        bool setShader(QString src);

    signals:
        /// Emitted when the list of user-settable uniform parameters to this
        /// shader change.
        ///
        /// The listening object should take this as a hint to update the UI.
        void paramsChanged();

        /// Emitted when the shader source code is updated.
        void shaderChanged();

        /// Emitted when a value of one of the current parameters changes
        ///
        /// Listeners should take this as a hint that the scene should be
        /// updated.
        void uniformValuesChanged();

    private slots:
        void setUniformValue(int value);
        void setUniformValue(double value);

    private:
        void setupParameters();

        double m_pointSize;
        double m_exposure;
        double m_contrast;
        int m_selector;
        typedef QMap<ShaderParam,ShaderParam::Variant> ParamMap;
        ParamMap m_params;
        std::unique_ptr<Shader> m_vertexShader;
        std::unique_ptr<Shader> m_fragmentShader;
        std::unique_ptr<QGLShaderProgram> m_shaderProgram;
};


#endif // SHADER_PROGRAM_H_INCLUDED
