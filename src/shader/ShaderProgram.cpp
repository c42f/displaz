// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "ShaderProgram.h"

#include "tinyformat.h"
#include "DragSpinBox.h"
#include "QtLogger.h"

#include <QFormLayout>
#include <QComboBox>
#include <QSlider>

ShaderProgram::ShaderProgram(QObject* parent)
    : QObject(parent),
    m_pointSize(5),
    m_exposure(1),
    m_contrast(1),
    m_selector(0)
{
}


bool paramOrderingLess(const QPair<ShaderParam,ShaderParam::Variant>& p1,
                       const QPair<ShaderParam,ShaderParam::Variant>& p2)
{
    return p1.first.ordering < p2.first.ordering;
}


void ShaderProgram::setupParameterUI(QWidget* parentWidget)
{
    QFormLayout* layout = new QFormLayout(parentWidget);
    QList<QPair<ShaderParam,ShaderParam::Variant> > paramsOrdered;
    for (auto p = m_params.begin(); p != m_params.end(); ++p)
    {
        paramsOrdered.push_back(qMakePair(p.key(), p.value()));
    }
    qSort(paramsOrdered.begin(), paramsOrdered.end(), paramOrderingLess);
    for (int i = 0; i < paramsOrdered.size(); ++i)
    {
        QWidget* edit = 0;
        const ShaderParam& parDesc = paramsOrdered[i].first;
        const ShaderParam::Variant& parValue = paramsOrdered[i].second;
        switch (parDesc.defaultValue.index())
        {
            case 0:
                {
                    bool expScaling = parDesc.kvPairs.value("scaling", "exponential").
                                      startsWith("exp");
                    double speed = parDesc.kvPairs.value("speed", "1").toDouble();
                    if (speed == 0)
                        speed = 1;
                    DragSpinBox* spin = new DragSpinBox(expScaling, speed, parentWidget);
                    double min = parDesc.getDouble("min", 0);
                    double max = parDesc.getDouble("max", 100);
                    if (expScaling)
                    {
                        // Prevent min or max == 0 for exp scaling which would be invalid
                        if (max == 0) max = 100;
                        if (min == 0) min = max > 0 ? 1e-8 : -1e-8;
                        spin->setDecimals((int)floor(std::max(0.0, -log(min)/log(10.0)) + 0.5) + 2);
                    }
                    else
                    {
                        double m = std::max(fabs(min), fabs(max));
                        spin->setDecimals((int)floor(std::max(0.0, -log(m)/log(10.0)) + 0.5) + 2);
                    }
                    spin->setMinimum(min);
                    spin->setMaximum(max);
                    spin->setValue(std::get<double>(parValue));
                    connect(spin, SIGNAL(valueChanged(double)),
                            this, SLOT(setUniformValue(double)));
                    edit = spin;
                }
                break;
            case 1:
                if (parDesc.kvPairs.contains("enum"))
                {
                    // Parameter is an enumeration variable
                    QComboBox* box = new QComboBox(parentWidget);
                    QStringList names = parDesc.kvPairs.value("enum").split('|');
                    box->insertItems(0, names);
                    box->setCurrentIndex(std::get<int>(parValue));
                    connect(box, SIGNAL(currentIndexChanged(int)),
                            this, SLOT(setUniformValue(int)));
                    edit = box;
                }
                else if (parDesc.kvPairs.contains("slider"))
                {
                    QSlider* slider = new QSlider(Qt::Horizontal, parentWidget);
                    slider->setMinimum(parDesc.getInt("min", 0));
                    slider->setMaximum(parDesc.getInt("max", 100));
                    slider->setValue(std::get<int>(parValue));
                    connect(slider, SIGNAL(valueChanged(int)),
                            this, SLOT(setUniformValue(int)));
                    edit = slider;
                }
                else
                {
                    // Parameter is a freely ranging integer
                    QSpinBox* spin = new QSpinBox(parentWidget);
                    spin->setMinimum(parDesc.getInt("min", 0));
                    spin->setMaximum(parDesc.getInt("max", 100));
                    spin->setValue(std::get<int>(parValue));
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


bool ShaderProgram::setShaderFromSourceFile(QString fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
    {
        //g_logger.error("Could not read Shader:\n%s", fileName.toStdString());
        return false;
    }
    return setShader(file.readAll());
}


bool ShaderProgram::setShader(QString src)
{
    std::unique_ptr<Shader> vertexShader(new Shader(QGLShader::Fragment));
    std::unique_ptr<Shader> fragmentShader(new Shader(QGLShader::Vertex));
    //tfm::printf("Shader source:\n###\n%s\n###\n", src.toStdString());
    QByteArray src_ba = src.toUtf8();
    if(!vertexShader->compileSourceCode(src_ba))
    {
        g_logger.error("Could not compile shader:\n%s",
                       vertexShader->shader()->log().toStdString());
        return false;
    }
    if(!fragmentShader->compileSourceCode(src_ba))
    {
        g_logger.error("Could not compile shader:\n%s",
                       fragmentShader->shader()->log().toStdString());
        return false;
    }
    std::unique_ptr<QGLShaderProgram> newProgram(new QGLShaderProgram());
    if (!newProgram->addShader(vertexShader->shader()) ||
        !newProgram->addShader(fragmentShader->shader()))
    {
        g_logger.error("Could not add shaders to program:\n%s",
                       newProgram->log().toStdString());
        return false;
    }
    if(!newProgram->link())
    {
        g_logger.error("Could not link shaders:\n%s",
                       newProgram->log().toStdString());
        return false;
    }
    // New shaders compiled & linked ok; swap out the old program for the new
    m_vertexShader = std::move(vertexShader);
    m_fragmentShader = std::move(fragmentShader);
    m_shaderProgram = std::move(newProgram);
    setupParameters();
    emit shaderChanged();
    return true;
}


void ShaderProgram::setUniformValue(double value)
{
    if (sender())
    {
        // Detect which uniform we're setting based on the sender's
        // name... ick!
        ShaderParam key(sender()->objectName().toUtf8().constData(), 0.0);
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
        ShaderParam key(sender()->objectName().toUtf8().constData(), 0);
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
    // TODO: Now that we have a single shared source for both shaders, should
    // clean up the way uniform parameters are parsed.
//    if (m_fragmentShader)
//        paramList.append(m_fragmentShader->uniforms());
    bool changed = paramList.size() != m_params.size();
    ParamMap newParams;
    for (int i = 0; i < paramList.size(); ++i)
    {
        paramList[i].ordering = i;
        ParamMap::const_iterator p = m_params.find(paramList[i]);
        if (p == m_params.end() || !(p.key() == paramList[i]))
            changed = true;
        ShaderParam::Variant value = paramList[i].defaultValue;
        // Keep the previous value for convenience
        if (p != m_params.end() && p.key().defaultValue.index() == paramList[i].defaultValue.index())
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
    for (ParamMap::const_iterator i = m_params.begin(); i != m_params.end(); ++i)
    {
        setUniform(i.key().name.data(), i.value());
    }
}


void ShaderProgram::setUniform(const char *name, const ShaderParam::Variant& value)
{
    switch (value.index())
    {
        case 0:
            m_shaderProgram->setUniformValue(name, (GLfloat) std::get<double>(value));
            break;
        case 1:
            m_shaderProgram->setUniformValue(name, (GLint) std::get<int>(value));
            break;
        case 2:
            // TODO
            break;
    }
}


bool ShaderProgram::getUniform(const char *name, ShaderParam::Variant& value)
{
    for (ParamMap::const_iterator i = m_params.begin(); i != m_params.end(); ++i)
    {
        if (i.key().name == QString(name))
        {
            value = i.value();
            return true;
        }
    }
    return false;
}


QByteArray ShaderProgram::shaderSource() const
{
    if (!m_vertexShader)
        return QByteArray("");
    return m_vertexShader->sourceCode();
}

