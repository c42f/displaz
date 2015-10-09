// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "shader.h"

#include "tinyformat.h"
#include "dragspinbox.h"
#include "qtlogger.h"

#include <QFormLayout>
#include <QComboBox>


/// Make shader #define flags for hardware or driver-dependent blacklisted
/// features.  Current list:
///
/// BROKEN_GL_FRAG_COORD
///
static QByteArray makeBlacklistDefines()
{
    bool isWindows = false;
#   ifdef _WIN32
    isWindows = true;
#   endif
    bool isIntel = false;
    // Extremely useful list of GL_VENDOR strings seen in the wild:
    // http://feedback.wildfiregames.com/report/opengl/feature/GL_VENDOR
    QString vendorStr = (const char*)glGetString(GL_VENDOR);
    if (vendorStr.contains("intel", Qt::CaseInsensitive))
        isIntel = true;
    QByteArray defines;
    // Blacklist use of gl_FragCoord/gl_FragDepth with Intel drivers on
    // windows.  For some reason, this interacts badly with any use of
    // gl_PointCoord, leading to gross rendering artifacts.
    if (isWindows && isIntel)
        defines += "#define BROKEN_GL_FRAG_COORD\n";
    return defines;
}


//------------------------------------------------------------------------------
// Shader implementation
bool Shader::compileSourceCode(const QByteArray& src)
{
    // src may contain parts which are shared for various shader types - set up
    // defines so we can tell which one we're compiling.
    QByteArray defines;
    switch (m_shader.shaderType())
    {
        case QOpenGLShader::Vertex:   defines += "#define VERTEX_SHADER\n";   break;
        case QOpenGLShader::Fragment: defines += "#define FRAGMENT_SHADER\n"; break;
    }
    defines += makeBlacklistDefines();
    QByteArray modifiedSrc = src;
    // Add defines.  Some shader compilers require #version to come first (even
    // before any #defines) so detect #version if it's present and put the
    // defines after that.
    int versionPos = src.indexOf("#version");
    if (versionPos != -1)
    {
        int insertPos = src.indexOf('\n', versionPos);
        if (insertPos == -1)
            insertPos = src.length();
        modifiedSrc.insert(insertPos+1, defines);
    }
    else
    {
        modifiedSrc = defines + src;
    }
    if (!m_shader.compileSourceCode(modifiedSrc))
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
        QByteArray typeStr = re.cap(1).toLatin1();
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
        ShaderParam param(type, re.cap(2).toLatin1(), defaultValue);
        QMap<QString, QString>& kvPairs = param.kvPairs;
        QStringList pairList = re.cap(4).split(';');
        for (int i = 0; i < pairList.size(); ++i)
        {
            QStringList keyAndVal = pairList[i].split('=');
            if (keyAndVal.size() != 2)
            {
                g_logger.warning("Could not parse metadata \"%s\" for shader variable %s",
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
ShaderProgram::ShaderProgram(const QOpenGLContext * context, QObject* parent)
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
                    spin->setValue(parValue.toDouble());
                    connect(spin, SIGNAL(valueChanged(double)),
                            this, SLOT(setUniformValue(double)));
                    edit = spin;
                }
                break;
            case ShaderParam::Int:
                if (parDesc.kvPairs.contains("enum"))
                {
                    // Parameter is an enumeration variable
                    QComboBox* box = new QComboBox(parentWidget);
                    QStringList names = parDesc.kvPairs.value("enum").split('|');
                    box->insertItems(0, names);
                    box->setCurrentIndex(parValue.toInt());
                    connect(box, SIGNAL(currentIndexChanged(int)),
                            this, SLOT(setUniformValue(int)));
                    edit = box;
                }
                else
                {
                    // Parameter is a freely ranging integer
                    QSpinBox* spin = new QSpinBox(parentWidget);
                    spin->setMinimum(parDesc.getInt("min", 0));
                    spin->setMaximum(parDesc.getInt("max", 100));
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


bool ShaderProgram::setShaderFromSourceFile(QString fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::ReadOnly))
        return false;
    return setShader(file.readAll());
}


bool ShaderProgram::setShader(QString src)
{
    std::unique_ptr<Shader> vertexShader(new Shader(QOpenGLShader::Fragment));
    std::unique_ptr<Shader> fragmentShader(new Shader(QOpenGLShader::Vertex));
    //tfm::printf("Shader source:\n###\n%s\n###\n", src.toStdString());
    if(!vertexShader->compileSourceCode(src.toLatin1()))
    {
        g_logger.error("Could not compile shader:\n%s",
                       vertexShader->shader()->log().toStdString());
        return false;
    }
    if(!fragmentShader->compileSourceCode(src.toLatin1()))
    {
        g_logger.error("Could not compile shader:\n%s",
                       fragmentShader->shader()->log().toStdString());
        return false;
    }
    std::unique_ptr<QOpenGLShaderProgram> newProgram(new QOpenGLShaderProgram());
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
        ShaderParam key(ShaderParam::Float, sender()->objectName().toLatin1());
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
        ShaderParam key(ShaderParam::Int, sender()->objectName().toLatin1());
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


void ShaderProgram::setContext(const QOpenGLContext* context)
{
    m_context = context;
    setShader(shaderSource());
}


QByteArray ShaderProgram::shaderSource() const
{
    if (!m_vertexShader)
        return QByteArray("");
    return m_vertexShader->sourceCode();
}


