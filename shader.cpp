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

#include "shader.h"

#include "tinyformat.h"
#include "dragspinbox.h"

#include <QtGui/QFormLayout>


//------------------------------------------------------------------------------
// Shader implementation
bool Shader::compileSourceCode(const QByteArray& src)
{
    m_source = src;
    // Search source code looking for uniform variables
    QRegExp re("uniform +([a-zA-Z_][a-zA-Z_0-9]*) +([a-zA-Z_][a-zA-Z_0-9]*) +=[^\n]*//# +([^\n]*)\\n");
    int pos = 0;
    while ((pos = re.indexIn(src, pos)) != -1)
    {
        pos += re.matchedLength();
        QByteArray typeStr = re.cap(1).toAscii();
//        std::cout << "typeStr = " << typeStr.data() << "\n";
        ShaderParam::Type type;
        if (typeStr == "float")
            type = ShaderParam::Float;
        else if (typeStr == "int")
            type = ShaderParam::Int;
        else if (typeStr == "vec3")
            type = ShaderParam::Vec3;
        else
            continue;
        m_uniforms.push_back(ShaderParam(type, re.cap(2).toAscii(),
                                         re.cap(3).toAscii()));
//        tfm::printf("Found shader uniform variable \"%s\"\n",
//                    m_uniforms.back().name.data());
    }
    return m_shader.compileSourceCode(src);
}


//------------------------------------------------------------------------------
// ShaderProgram implementation
ShaderProgram::ShaderProgram(const QGLContext * context, QObject* parent)
    : QObject(parent),
    m_context(context),
    m_pointSize(5),
    m_exposure(1),
    m_contrast(1),
    m_selector(0)
{ }


void ShaderProgram::setupParameterUI(QWidget* parentWidget)
{
    QFormLayout* layout = new QFormLayout(parentWidget);
    DragSpinBox* pointSizeEdit = new DragSpinBox(parentWidget);
    layout->addRow(tr("Point &Size:"), pointSizeEdit);
    pointSizeEdit->setDecimals(2);
    pointSizeEdit->setMinimum(1);
    pointSizeEdit->setMaximum(200);
    connect(pointSizeEdit, SIGNAL(valueChanged(double)),
            this, SLOT(setPointSize(double)));
    pointSizeEdit->setValue(10);

    DragSpinBox* exposureEdit = new DragSpinBox(parentWidget);
    layout->addRow(tr("Exposure:"), exposureEdit);
    exposureEdit->setDecimals(5);
    exposureEdit->setMinimum(0.0001);
    exposureEdit->setMaximum(10000);
    connect(exposureEdit, SIGNAL(valueChanged(double)),
            this, SLOT(setExposure(double)));
    exposureEdit->setValue(1);

    DragSpinBox* contrastEdit = new DragSpinBox(parentWidget);
    layout->addRow(tr("Contrast:"), contrastEdit);
    contrastEdit->setDecimals(4);
    contrastEdit->setMinimum(0.001);
    contrastEdit->setMaximum(1000);
    connect(contrastEdit, SIGNAL(valueChanged(double)),
            this, SLOT(setContrast(double)));
    contrastEdit->setValue(1);

    QSpinBox* selectorEdit = new QSpinBox(parentWidget);
    layout->addRow(tr("Selector:"), selectorEdit);
    selectorEdit->setMinimum(-1);
    selectorEdit->setMaximum(1000);
    connect(selectorEdit, SIGNAL(valueChanged(int)),
            this, SLOT(setSelector(int)));
    selectorEdit->setValue(0);

#if 0
    // Collect full set of parameters
    QFormLayout* layout = new QFormLayout(parentWidget);
    for (int i = 0; i < params.size(); ++i)
    {
        QWidget* editor = 0;
        switch (params[i].type)
        {
            case ShaderParam::Float:
                {
                    DragSpinBox* box = new DragSpinBox(parentWidget);
                    double min = 0.1;
                    double max = 200;
                    box->setDecimals((int)floor(log(min)/log(10) + 0.5) + 2);
                    box->setMinimum(min);
                    box->setMaximum(max);
                    box->connect
                    editor = box;
                }
                break;
            case ShaderParam::Int:
                {
                    QSpinBox* box = new QSpinBox(parentWidget);
                    layout->addRow(tr("Selector:"), box);
                    box->setMinimum(-1);
                    box->setMaximum(1000);
                    editor = box;
                }
                break;
            default:
                continue;
        }
        layout->addRow(tr(params[i].uiName) + ":", editor);

        DragSpinBox* pointSizeEdit = new DragSpinBox(parentWidget);
        layout->addRow(tr(params[i].uiName) + ":", pointSizeEdit);
        pointSizeEdit->setDecimals(2);
        pointSizeEdit->setMinimum(1);
        pointSizeEdit->setMaximum(200);
        connect(pointSizeEdit, SIGNAL(valueChanged(double)),
                this, SLOT(setPointSize(double)));
        pointSizeEdit->setValue(10);

        QSpinBox* selectorEdit = new QSpinBox(parentWidget);
        layout->addRow(tr("Selector:"), selectorEdit);
        selectorEdit->setMinimum(-1);
        selectorEdit->setMaximum(1000);
        connect(selectorEdit, SIGNAL(valueChanged(int)),
                this, SLOT(setSelector(int)));
        selectorEdit->setValue(0);

    }
#endif
}


void ShaderProgram::setVertexShader(QString src)
{
    std::unique_ptr<Shader> shader(new Shader(QGLShader::Vertex, m_context));
    //tfm::printf("Shader source:\n###\n%s\n###\n", src.toStdString());
    if(!shader->compileSourceCode(src.toAscii()))
    {
        tfm::printf("Error compiling shader:\n%s\n",
                    shader->shader()->log().toStdString());
        return;
    }
    std::unique_ptr<QGLShaderProgram> newProgram(new QGLShaderProgram(m_context));
    if (!newProgram->addShader(shader->shader()) ||
        (m_fragmentShader && !newProgram->addShader(m_fragmentShader->shader())))
    {
        tfm::printf("Error adding shaders to program:\n%s\n",
                    newProgram->log().toStdString());
        return;
    }
    if(!newProgram->link())
    {
        tfm::printf("Error linking shaders:\n%s\n",
                    newProgram->log().toStdString());
        return;
    }
    // New shader compiled & linked ok; swap out the old program for the new
    m_vertexShader = std::move(shader);
    m_shaderProgram = std::move(newProgram);
    setupParameters();
}


void ShaderProgram::setFragmentShader(QString src)
{
    std::unique_ptr<Shader> shader(new Shader(QGLShader::Fragment, m_context));
    if(!shader->compileSourceCode(src.toAscii()))
    {
        tfm::printf("Error compiling shader:\n%s\n",
                    shader->shader()->log().toStdString());
        return;
    }
    std::unique_ptr<QGLShaderProgram> newProgram(new QGLShaderProgram(m_context));
    if (!newProgram->addShader(shader->shader()) ||
        (m_vertexShader && !newProgram->addShader(m_vertexShader->shader())))
    {
        tfm::printf("Error adding shaders to program:\n%s\n",
                    newProgram->log().toStdString());
        return;
    }
    if(!newProgram->link())
    {
        tfm::printf("Error linking shaders:\n%s\n",
                    newProgram->log().toStdString());
        return;
    }
    // New shader compiled & linked ok; swap out the old program for the new
    m_fragmentShader = std::move(shader);
    m_shaderProgram = std::move(newProgram);
    setupParameters();
}


// For sorting shader parameters
bool operator<(const ShaderParam& p1, const ShaderParam& p2)
{
    return p1.name < p2.name;
}

struct ShaderParamUniquer
{
    bool operator()(const ShaderParam& p1, const ShaderParam& p2)
    {
        return p1.name == p2.name;
    }
};


void ShaderProgram::setupParameters()
{
#if 0
    QList<ShaderParam> params;
    if (m_vertexShader)
        params.append(m_vertexShader->uniforms());
    if (m_fragmentShader)
        params.append(m_fragmentShader->uniforms());
    qSort(params.begin(), params.end());
    // Erase any duplicate names
    params.erase(std::unique(params.begin(), params.end(),
                             ShaderParamUniquer()), params.end());
    if (params != m_params)
    {
        m_params = params;
        emit paramsChanged();
    }
#endif
    emit paramsChanged();
}


void ShaderProgram::setUniforms()
{
    m_shaderProgram->setUniformValue("exposure", (float)m_exposure);
    m_shaderProgram->setUniformValue("contrast", (float)m_contrast);
    m_shaderProgram->setUniformValue("selector", m_selector);
    m_shaderProgram->setUniformValue("pointSize", (float)m_pointSize);
}


void ShaderProgram::setPointSize(double size)
{
    m_pointSize = size;
    emit uniformValuesChanged();
}


void ShaderProgram::setExposure(double intensity)
{
    m_exposure = intensity;
    emit uniformValuesChanged();
}


void ShaderProgram::setContrast(double power)
{
    m_contrast = power;
    emit uniformValuesChanged();
}


void ShaderProgram::setSelector(int sel)
{
    m_selector = sel;
    emit uniformValuesChanged();
}


QByteArray ShaderProgram::vertexShader() const
{
    if (!m_vertexShader)
        return QByteArray("");
    return m_vertexShader->sourceCode();
}


QByteArray ShaderProgram::fragmentShader() const
{
    if (!m_fragmentShader)
        return QByteArray("");
    return m_fragmentShader->sourceCode();
}


