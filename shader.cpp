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
    if (!m_shader.compileSourceCode(src))
        return false;
    m_source = src;
    // Search source code looking for uniform variables
    QStringList lines = QString(src).split('\n');
    QRegExp re("uniform +([a-zA-Z_][a-zA-Z_0-9]*) +([a-zA-Z_][a-zA-Z_0-9]*) +=(.+); *//# *(.*)",
               Qt::CaseSensitive, QRegExp::RegExp2);
    for (int lineIdx = 0; lineIdx < lines.size(); ++lineIdx)
    {
        if (!re.exactMatch(lines[lineIdx]))
            continue;
        QByteArray typeStr = re.cap(1).toAscii();
        QVariant defaultValue;
        ShaderParam::Type type;
        if (typeStr == "float")
        {
            type = ShaderParam::Float;
            defaultValue = re.cap(3).trimmed().toDouble();
        }
        else if (typeStr == "int")
        {
            type = ShaderParam::Int;
            defaultValue = re.cap(3).trimmed().toInt();
        }
        else if (typeStr == "vec3")
        {
            type = ShaderParam::Vec3;
            //defaultValue = re.cap(3).toDouble(); // FIXME
        }
        else
            continue;
        ShaderParam param(type, re.cap(2).toAscii(), defaultValue);
        QMap<QString, QString>& kvPairs = param.kvPairs;
        QStringList pairList = re.cap(4).split(';');
        for (int i = 0; i < pairList.size(); ++i)
        {
            QStringList keyAndVal = pairList[i].split('=');
            if (keyAndVal.size() != 2)
            {
                tfm::printf("Could not parse metadata \"%s\" for shader variable %s\n",
                            pairList[i].toStdString(), param.name.data());
                continue;
            }
            kvPairs[keyAndVal[0].trimmed()] = keyAndVal[1].trimmed();
        }
        m_uniforms.push_back(param);
    }
    /*
    // Debug: print out what we found
    for (int i = 0; i < m_uniforms.size(); ++i)
    {
        tfm::printf("Found shader uniform \"%s\" with metadata\n",
                    m_uniforms[i].name.data());
        const QMap<QString, QString>& kvPairs = m_uniforms[i].kvPairs;
        for (QMap<QString,QString>::const_iterator i = kvPairs.begin();
             i != kvPairs.end(); ++i)
        {
            tfm::printf("  %s = \"%s\"\n", i.key().toStdString(),
                        i.value().toStdString());
        }
    }
    */
    return true;
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


bool paramOrderingLess(const QPair<ShaderParam,QVariant>& p1,
                       const QPair<ShaderParam,QVariant>& p2)
{
    return p1.first.ordering < p2.first.ordering;
}

void ShaderProgram::setupParameterUI(QWidget* parentWidget)
{
    QFormLayout* layout = new QFormLayout(parentWidget);
    QList<QPair<ShaderParam,QVariant> > paramsOrdered;
    for (ParamMap::iterator p = m_params.begin(); p != m_params.end(); ++p)
        paramsOrdered.push_back(qMakePair(p.key(), p.value()));
    qSort(paramsOrdered.begin(), paramsOrdered.end(), paramOrderingLess);
    for (int i = 0; i < paramsOrdered.size(); ++i)
    {
        QWidget* edit = 0;
        const ShaderParam& parDesc = paramsOrdered[i].first;
        const QVariant& parValue = paramsOrdered[i].second;
        switch (parDesc.type)
        {
            case ShaderParam::Float:
                {
                    DragSpinBox* spin = new DragSpinBox(parentWidget);
                    double min = parDesc.min();
                    spin->setDecimals((int)floor(std::max(0.0, -log(min)/log(10.0)) + 0.5) + 2);
                    spin->setMinimum(min);
                    spin->setMaximum(parDesc.max());
                    spin->setValue(parValue.toDouble());
                    connect(spin, SIGNAL(valueChanged(double)),
                            this, SLOT(setUniformValue(double)));
                    edit = spin;
                }
                break;
            case ShaderParam::Int:
                {
                    QSpinBox* spin = new QSpinBox(parentWidget);
                    spin->setMinimum((int)parDesc.min());
                    spin->setMaximum((int)parDesc.max());
                    spin->setValue(parValue.toInt());
                    connect(spin, SIGNAL(valueChanged(int)),
                            this, SLOT(setUniformValue(int)));
                    edit = spin;
                }
                break;
            default:
                // FIXME
                continue;
        }
        layout->addRow(parDesc.uiName() + ":", edit);
        edit->setObjectName(parDesc.name);
    }
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
    emit shaderChanged();
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
    emit shaderChanged();
}


void ShaderProgram::setUniformValue(double value)
{
    if (sender())
    {
        // Detect which uniform we're setting based on the sender's
        // name... ick!
        ShaderParam key(ShaderParam::Float, sender()->objectName().toAscii());
        ParamMap::iterator i = m_params.find(key);
        if (i == m_params.end())
        {
            assert(0 && "Couldn't find parameter");
            return;
        }
        i.value() = value;
        emit uniformValuesChanged();
    }
}


void ShaderProgram::setUniformValue(int value)
{
    if (sender())
    {
        // Detect which uniform we're setting based on the sender's
        // name... ick!
        ShaderParam key(ShaderParam::Int, sender()->objectName().toAscii());
        ParamMap::iterator i = m_params.find(key);
        if (i == m_params.end())
        {
            assert(0 && "Couldn't find parameter");
            return;
        }
        i.value() = value;
        emit uniformValuesChanged();
    }
}


void ShaderProgram::setupParameters()
{
    QList<ShaderParam> paramList;
    if (m_vertexShader)
        paramList.append(m_vertexShader->uniforms());
    if (m_fragmentShader)
        paramList.append(m_fragmentShader->uniforms());
    bool changed = paramList.size() != m_params.size();
    ParamMap newParams;
    for (int i = 0; i < paramList.size(); ++i)
    {
        paramList[i].ordering = i;
        ParamMap::const_iterator p = m_params.find(paramList[i]);
        if (p == m_params.end() || !(p.key() == paramList[i]))
            changed = true;
        QVariant value;
        value = paramList[i].defaultValue;
        // Keep the previous value for convenience
        if (p != m_params.end() && p.key().type == paramList[i].type)
            value = p.value();
        if (!newParams.contains(paramList[i]))
            newParams.insert(paramList[i], value);
    }
    if (changed)
    {
        m_params = newParams;
        emit paramsChanged();
    }
}


void ShaderProgram::setUniforms()
{
    for (ParamMap::const_iterator i = m_params.begin();
         i != m_params.end(); ++i)
    {
        const ShaderParam& param = i.key();
        QVariant value = i.value();
        switch (param.type)
        {
            case ShaderParam::Float:
                m_shaderProgram->setUniformValue(param.name.data(),
                                                 (GLfloat)value.toDouble());
                break;
            case ShaderParam::Int:
                m_shaderProgram->setUniformValue(param.name.data(),
                                                 (GLint)value.toInt());
                break;
            case ShaderParam::Vec3:
                // FIXME
                break;
        }
    }
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


