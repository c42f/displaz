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

#include "glutil.h"
#include "tinyformat.h"

TransformState TransformState::translate(const Imath::V3d& offset) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().setTranslation(offset) * modelViewMatrix;
    return res;
}


static void setUniform(GLuint prog, const char* name, const M44d& mat)
{
    GLint loc = glGetUniformLocation(prog, name);
    if (loc == -1)
        return;
    M44f M(mat);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &M[0][0]);
}


void TransformState::setUniforms(GLuint prog) const
{
    // Note: The matrices must have a sensible representation in float32
    // precision by the time they get here.
    setUniform(prog, "projectionMatrix", projMatrix);
    setUniform(prog, "modelViewMatrix", modelViewMatrix);
    M44d mvproj = modelViewMatrix * projMatrix;
    setUniform(prog, "modelViewProjectionMatrix", mvproj);
}


void TransformState::load() const
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix(M44f(projMatrix));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrix(M44f(modelViewMatrix));
}


//------------------------------------------------------------------------------

void drawBoundingBox(const TransformState& transState,
                     const Imath::Box3d& bbox,
                     const Imath::C3f& col)
{
    // Transform to box min for stability with large offsets
    TransformState trans2 = transState.translate(bbox.min);
    Imath::Box3f box2(V3f(0), V3f(bbox.max - bbox.min));
    drawBoundingBox(trans2, box2, col);
}


void drawBoundingBox(const TransformState& transState,
                     const Imath::Box3f& bbox,
                     const Imath::C3f& col)
{
    transState.load();
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(col.x, col.y, col.z);
    glLineWidth(1);
    GLfloat verts[] = {
        bbox.min.x, bbox.min.y, bbox.min.z,
        bbox.min.x, bbox.max.y, bbox.min.z,
        bbox.max.x, bbox.max.y, bbox.min.z,
        bbox.max.x, bbox.min.y, bbox.min.z,
        bbox.min.x, bbox.min.y, bbox.max.z,
        bbox.min.x, bbox.max.y, bbox.max.z,
        bbox.max.x, bbox.max.y, bbox.max.z,
        bbox.max.x, bbox.min.y, bbox.max.z
    };
    unsigned char inds[] = {
        // rows: bottom, sides, top
        0,1, 1,2, 2,3, 3,0,
        0,4, 1,5, 2,6, 3,7,
        4,5, 5,6, 6,7, 7,4
    };
    // TODO: Use shaders here
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glDrawElements(GL_LINES, sizeof(inds)/sizeof(inds[0]),
                   GL_UNSIGNED_BYTE, inds);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}


/// Get information about openGL type
bool getGlTypeInfo(int type, const char*& name, int& rows, int& cols, TypeSpec::Type& tbase)
{
    switch(type)
    {
        case GL_FLOAT:             rows = 1; cols = 1; tbase = TypeSpec::Float; name = "GL_FLOAT";             break;
        case GL_FLOAT_VEC2:        rows = 2; cols = 1; tbase = TypeSpec::Float; name = "GL_FLOAT_VEC2";        break;
        case GL_FLOAT_VEC3:        rows = 3; cols = 1; tbase = TypeSpec::Float; name = "GL_FLOAT_VEC3";        break;
        case GL_FLOAT_VEC4:        rows = 4; cols = 1; tbase = TypeSpec::Float; name = "GL_FLOAT_VEC4";        break;
        case GL_FLOAT_MAT2:        rows = 2; cols = 2; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT2";        break;
        case GL_FLOAT_MAT3:        rows = 3; cols = 3; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT3";        break;
        case GL_FLOAT_MAT4:        rows = 4; cols = 4; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT4";        break;
        case GL_FLOAT_MAT2x3:      rows = 2; cols = 3; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT2x3";      break;
        case GL_FLOAT_MAT2x4:      rows = 2; cols = 4; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT2x4";      break;
        case GL_FLOAT_MAT3x2:      rows = 3; cols = 2; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT3x2";      break;
        case GL_FLOAT_MAT3x4:      rows = 3; cols = 4; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT3x4";      break;
        case GL_FLOAT_MAT4x2:      rows = 4; cols = 2; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT4x2";      break;
        case GL_FLOAT_MAT4x3:      rows = 4; cols = 3; tbase = TypeSpec::Float; name = "GL_FLOAT_MAT4x3";      break;
        case GL_INT:               rows = 1; cols = 1; tbase = TypeSpec::Int;   name = "GL_INT";               break;
        case GL_INT_VEC2:          rows = 2; cols = 1; tbase = TypeSpec::Int;   name = "GL_INT_VEC2";          break;
        case GL_INT_VEC3:          rows = 3; cols = 1; tbase = TypeSpec::Int;   name = "GL_INT_VEC3";          break;
        case GL_INT_VEC4:          rows = 4; cols = 1; tbase = TypeSpec::Int;   name = "GL_INT_VEC4";          break;
        case GL_UNSIGNED_INT:      rows = 1; cols = 1; tbase = TypeSpec::Uint;  name = "GL_UNSIGNED_INT";      break;
        case GL_UNSIGNED_INT_VEC2: rows = 2; cols = 1; tbase = TypeSpec::Uint;  name = "GL_UNSIGNED_INT_VEC2"; break;
        case GL_UNSIGNED_INT_VEC3: rows = 3; cols = 1; tbase = TypeSpec::Uint;  name = "GL_UNSIGNED_INT_VEC3"; break;
        case GL_UNSIGNED_INT_VEC4: rows = 4; cols = 1; tbase = TypeSpec::Uint;  name = "GL_UNSIGNED_INT_VEC4"; break;
        case GL_DOUBLE:            rows = 1; cols = 1; tbase = TypeSpec::Float; name = "GL_DOUBLE";            break;
        default:  return false; break;
    }
    return true;
}


std::vector<ShaderAttribute> activeShaderAttributes(GLuint prog)
{
    std::vector<ShaderAttribute> attrs;
    GLint numActiveAttrs = 0;
    glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &numActiveAttrs);
    GLint maxAttrLength = 0;
    glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttrLength);
    std::vector<GLchar> nameData(maxAttrLength);
    for (int attrIdx = 0; attrIdx < numActiveAttrs; ++attrIdx)
    {
        GLint arraySize = 0;
        GLenum type = 0;
        GLsizei nameLength = 0;
        glGetActiveAttrib(prog, attrIdx, (GLsizei)nameData.size(),
                          &nameLength, &arraySize, &type, &nameData[0]);
        const char* typeName = 0;
        int rows = 1;
        int cols = 1;
        TypeSpec::Type tbase = TypeSpec::Unknown;
        getGlTypeInfo(type, typeName, rows, cols, tbase);
        ShaderAttribute attr;
        attr.type = type;
        attr.count = arraySize;
        attr.name = std::string(nameData.data(), nameLength);
        attr.rows = rows;
        attr.cols = cols;
        attr.baseType = tbase;
        attr.location = glGetAttribLocation(prog, attr.name.c_str());
        attrs.push_back(attr);
    }
    return attrs;
}


const ShaderAttribute* findAttr(const std::string& name,
                                const std::vector<ShaderAttribute>& attrs)
{
    for (size_t j = 0; j < attrs.size(); ++j)
    {
        if (attrs[j].name == name)
        {
            // Could ensure compatibility between point field and shader field
            // (type, number of components etc).  It's fairly convenient to not be
            // too strict about this however - it seems that the GL driver will
            // just discard any excess components passed.
            // TODO: At least make this a warning somehow
            return &attrs[j];
        }
    }
    return nullptr;
}



void printActiveShaderAttributes(GLuint prog)
{
    std::vector<ShaderAttribute> attrs = activeShaderAttributes(prog);
    for (size_t attrIdx = 0; attrIdx < attrs.size(); ++attrIdx)
    {
        const ShaderAttribute& attr = attrs[attrIdx];
        const char* typeName = 0;
        int rows = 1;
        int cols = 1;
        TypeSpec::Type tbase = TypeSpec::Unknown;
        getGlTypeInfo(attr.type, typeName, rows, cols, tbase);
        tfm::printf("   %s[%d] %s\n", typeName, attr.count, attr.name);
    }
}

