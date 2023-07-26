// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "Shader.h"

#include "tinyformat.h"
#include "QtLogger.h"

#include <QFormLayout>
#include <QComboBox>


/// Make shader #define flags for hardware or driver-dependent blacklisted
/// features.  Current list:
///
/// BROKEN_GL_FRAG_COORD
///
static QByteArray makeBlacklistDefines()
{
    QByteArray defines;
#   ifdef _WIN32
    bool isIntel = false;
    // Extremely useful list of GL_VENDOR strings seen in the wild:
    // http://feedback.wildfiregames.com/report/opengl/feature/GL_VENDOR
    QString vendorStr = (const char*)glGetString(GL_VENDOR);
    if (vendorStr.contains("intel", Qt::CaseInsensitive))
        isIntel = true;
    // Blacklist use of gl_FragCoord/gl_FragDepth with Intel drivers on
    // windows.  For some reason, this interacts badly with any use of
    // gl_PointCoord, leading to gross rendering artifacts.
    if (isIntel)
        defines += "#define BROKEN_GL_FRAG_COORD\n";
#endif
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
        case QGLShader::Vertex:   defines += "#define VERTEX_SHADER\n";   break;
        case QGLShader::Fragment: defines += "#define FRAGMENT_SHADER\n"; break;
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
        QByteArray typeStr = re.cap(1).toUtf8().constData();
        ShaderParam::Variant defaultValue;
        if (typeStr == "float")
        {
            defaultValue = re.cap(3).trimmed().toDouble();
        }
        else if (typeStr == "int")
        {
            defaultValue = re.cap(3).trimmed().toInt();
        }
        else if (typeStr == "vec3")
        {
            //defaultValue = re.cap(3).toDouble(); // FIXME
        }
        else
            continue;
        ShaderParam param(re.cap(2).toUtf8().constData(), defaultValue);
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
