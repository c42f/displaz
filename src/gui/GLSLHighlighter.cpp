// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "GLSLHighlighter.h"

#include <QColor>

GLSLHighlighter::GLSLHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    m_commentBegin = QRegExp("/\\*");
    m_commentEnd = QRegExp("\\*/");
    // Keywords
    addRule("#a71d5d", TextStyle::Plain, "\\b(defined|attribute|const|uniform|varying|layout|centroid|flat|smooth|noperspective|patch|sample|break|continue|do|for|while|switch|case|default|if|else|subroutine|in|out|inout|float|double|int|void|bool|true|false|invariant|discard|return|mat2|mat3|mat4|dmat2|dmat3|dmat4|mat2x2|mat2x3|mat2x4|dmat2x2|dmat2x3|dmat2x4|mat3x2|mat3x3|mat3x4|dmat3x2|dmat3x3|dmat3x4|mat4x2|mat4x3|mat4x4|dmat4x2|dmat4x3|dmat4x4|vec2|vec3|vec4|ivec2|ivec3|ivec4|bvec2|bvec3|bvec4|dvec2|dvec3|dvec4|uint|uvec2|uvec3|uvec4|lowp|mediump|highp|precision|sampler1D|sampler2D|sampler3D|samplerCube|sampler1DShadow|sampler2DShadow|samplerCubeShadow|sampler1DArray|sampler2DArray|sampler1DArrayShadow|sampler2DArrayShadow|isampler1D|isampler2D|isampler3D|isamplerCube|isampler1DArray|isampler2DArray|usampler1D|usampler2D|usampler3D|usamplerCube|usampler1DArray|usampler2DArray|sampler2DRect|sampler2DRectShadow|isampler2DRect|usampler2DRect|samplerBuffer|isamplerBuffer|usamplerBuffer|sampler2DMS|isampler2DMS|usampler2DMS|sampler2DMSArray|isampler2DMSArray|usampler2DMSArray|samplerCubeArray|samplerCubeArrayShadow|isamplerCubeArray|usamplerCubeArray|struct|common|partition|active|asm|class|union|enum|typedef|template|this|packed|goto|inline|noinline|volatile|public|static|extern|external|interface|long|short|half|fixed|unsigned|superp|input|output|hvec2|hvec3|hvec4|fvec2|fvec3|fvec4|sampler3DRect|filter|image1D|image2D|image3D|imageCube|iimage1D|iimage2D|iimage3D|iimageCube|uimage1D|uimage2D|uimage3D|uimageCube|image1DArray|image2DArray|iimage1DArray|iimage2DArray|uimage1DArray|uimage2DArray|image1DShadow|image2DShadow|image1DArrayShadow|image2DArrayShadow|imageBuffer|iimageBuffer|uimageBuffer|sizeof|cast|namespace|using|row_major)\\b");
    // Builtin variables
    addRule("#0086b3", TextStyle::Plain, "\\b(gl_BackColor|gl_BackLightModelProduct|gl_BackLightProduct|gl_BackMaterial|gl_BackSecondaryColor|gl_ClipDistance|gl_ClipPlane|gl_ClipVertex|gl_Color|gl_DepthRange|gl_DepthRangeParameters|gl_EyePlaneQ|gl_EyePlaneR|gl_EyePlaneS|gl_EyePlaneT|gl_Fog|gl_FogCoord|gl_FogFragCoord|gl_FogParameters|gl_FragColor|gl_FragCoord|gl_FragDat|gl_FragDept|gl_FrontColor|gl_FrontFacing|gl_FrontLightModelProduct|gl_FrontLightProduct|gl_FrontMaterial|gl_FrontSecondaryColor|gl_InstanceID|gl_Layer|gl_LightModel|gl_LightModelParameters|gl_LightModelProducts|gl_LightProducts|gl_LightSource|gl_LightSourceParameters|gl_MaterialParameters|gl_ModelViewMatrix|gl_ModelViewMatrixInverse|gl_ModelViewMatrixInverseTranspose|gl_ModelViewMatrixTranspose|gl_ModelViewProjectionMatrix|gl_ModelViewProjectionMatrixInverse|gl_ModelViewProjectionMatrixInverseTranspose|gl_ModelViewProjectionMatrixTranspose|gl_MultiTexCoord[0-7]|gl_Normal|gl_NormalMatrix|gl_NormalScale|gl_ObjectPlaneQ|gl_ObjectPlaneR|gl_ObjectPlaneS|gl_ObjectPlaneT|gl_Point|gl_PointCoord|gl_PointParameters|gl_PointSize|gl_Position|gl_PrimitiveIDIn|gl_ProjectionMatrix|gl_ProjectionMatrixInverse|gl_ProjectionMatrixInverseTranspose|gl_ProjectionMatrixTranspose|gl_SecondaryColor|gl_TexCoord|gl_TextureEnvColor|gl_TextureMatrix|gl_TextureMatrixInverse|gl_TextureMatrixInverseTranspose|gl_TextureMatrixTranspose|gl_Vertex|gl_VertexID|__LINE__|__FILE__|__VERSION__|GL_core_profile|GL_es_profile|GL_compatibility_profile|gl_MaxClipPlanes|gl_MaxCombinedTextureImageUnits|gl_MaxDrawBuffers|gl_MaxFragmentUniformComponents|gl_MaxLights|gl_MaxTextureCoords|gl_MaxTextureImageUnits|gl_MaxTextureUnits|gl_MaxVaryingFloats|gl_MaxVertexAttribs|gl_MaxVertexTextureImageUnits|gl_MaxVertexUniformComponents|VERTEX_SHADER|FRAGMENT_SHADER)\\b");
    // Builtin functions
    addRule("#0086b3", TextStyle::Plain, "\\b(abs|acos|all|any|asin|atan|ceil|clamp|cos|cross|degrees|dFdx|dFdy|distance|dot|equal|exp|exp2|faceforward|floor|fract|ftransform|fwidth|greaterThan|greaterThanEqual|inversesqrt|length|lessThan|lessThanEqual|log|log2|matrixCompMult|max|min|mix|mod|noise[1-4]|normalize|not|notEqual|outerProduct|pow|radians|reflect|refract|shadow1D|shadow1DLod|shadow1DProj|shadow1DProjLod|shadow2D|shadow2DLod|shadow2DProj|shadow2DProjLod|sign|sin|smoothstep|sqrt|step|tan|texture1D|texture1DLod|texture1DProj|texture1DProjLod|texture2D|texture2DLod|texture2DProj|texture2DProjLod|texture3D|texture3DLod|texture3DProj|texture3DProjLod|textureCube|textureCubeLod|transpose|void|bool|int|uint|float|double|vec[2|3|4]|dvec[2|3|4]|bvec[2|3|4]|ivec[2|3|4]|uvec[2|3|4]|mat[2|3|4]|mat2x2|mat2x3|mat2x4|mat3x2|mat3x3|mat3x4|mat4x2|mat4x3|mat4x4|dmat2|dmat3|dmat4|dmat2x2|dmat2x3|dmat2x4|dmat3x2|dmat3x3|dmat3x4|dmat4x2|dmat4x3|dmat4x4|sampler[1|2|3]D|image[1|2|3]D|samplerCube|imageCube|sampler2DRect|image2DRect|sampler[1|2]DArray|image[1|2]DArray|samplerBuffer|imageBuffer|sampler2DMS|image2DMS|sampler2DMSArray|image2DMSArray|samplerCubeArray|imageCubeArray|sampler[1|2]DShadow|sampler2DRectShadow|sampler[1|2]DArrayShadow|samplerCubeShadow|samplerCubeArrayShadow|isampler[1|2|3]D|iimage[1|2|3]D|isamplerCube|iimageCube|isampler2DRect|iimage2DRect|isampler[1|2]DArray|iimage[1|2]DArray|isamplerBuffer|iimageBuffer|isampler2DMS|iimage2DMS|isampler2DMSArray|iimage2DMSArray|isamplerCubeArray|iimageCubeArray|atomic_uint|usampler[1|2|3]D|uimage[1|2|3]D|usamplerCube|uimageCube|usampler2DRect|uimage2DRect|usampler[1|2]DArray|uimage[1|2]DArray|usamplerBuffer|uimageBuffer|usampler2DMS|uimage2DMS|usampler2DMSArray|uimage2DMSArray|usamplerCubeArray|uimageCubeArray|struct)(?=\\()");
    // Preprocessor directives
    addRule("#a71d5d", TextStyle::Plain, "\\s*#\\s*(define|undef|if|ifdef|ifndef|else|elif|endif|error|pragma|extension|version|line)\\b");
    // Shader parameter comments
    addRule("#183691", TextStyle::Plain, "//#[^\n]*");
    // Operators
    addRule("#a71d5d", TextStyle::Plain, "[%&\\+\\-=/\\|\\.\\*:><!\\?~\\^]");
    addRule("#444444", TextStyle::Plain, ";");
    // Literal
    QString boolPattern = "true|false";
    QString intPattern = "[0-9]+u?";
    QString floatPattern = "[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?";
    addRule("#0086b3", TextStyle::Plain,
        "\\b("
        + boolPattern + "|"
        + intPattern + "|"
        + floatPattern
        + ")\\b");
    // Strings
    addRule("#183691", TextStyle::Plain, "\".*\"");
    // Comments
    addRule("#969896", TextStyle::Italic, "//");
    addRule("#969896", TextStyle::Italic, "//[^#][^\n]*");
    m_comment.setForeground(QColor("#969896"));
    m_comment.setFontItalic(true);
    // Shader parameter comment begin
    addRule("#969896", TextStyle::Italic, "//#");
}

void GLSLHighlighter::highlightBlock(const QString& text)
{
    foreach (const Rule& rule, m_rules)
    {
        int index = rule.regexp.indexIn(text);
        while (index >= 0)
        {
            int length = rule.regexp.matchedLength();
            setFormat(index, length, rule.format);
            index = rule.regexp.indexIn(text, index + length);
        }
    }
    setCurrentBlockState(BlockState::Default);
    // Highlight multiline comments
    int beginIndex = 0;
    if (previousBlockState() == BlockState::Default)
        beginIndex = m_commentBegin.indexIn(text);
    while (beginIndex >= 0)
    {
        int endIndex = m_commentEnd.indexIn(text, beginIndex);
        int length;
        if (endIndex >= 0)
        {
            length = endIndex - beginIndex + m_commentEnd.matchedLength();
        }
        else
        {
            setCurrentBlockState(BlockState::MultilineComment);
            length = text.length() - beginIndex;
        }
        setFormat(beginIndex, length, m_comment);
        beginIndex = m_commentBegin.indexIn(text, beginIndex + length);
    }
}

void GLSLHighlighter::addRule(const QString& color, TextStyle style,
                              const QString& pattern)
{
    Rule rule;
    QTextCharFormat format;
    format.setForeground(QColor(color));
    format.setFontItalic(style == TextStyle::Italic);
    rule.format = format;
    rule.regexp = QRegExp(pattern);
    m_rules.append(rule);
}
