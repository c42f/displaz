// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "glutil.h"
#include "tinyformat.h"

TransformState TransformState::translate(const Imath::V3d& offset) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().setTranslation(offset) * modelViewMatrix;
    return res;
}

TransformState TransformState::scale(const Imath::V3d& scalar) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().setScale(scalar) * modelViewMatrix;
    return res;
}

TransformState TransformState::rotate(const Imath::V4d& rotation) const
{
    TransformState res(*this);
    res.modelViewMatrix =  M44d().rotate(V3d(rotation.x*rotation.w,rotation.y*rotation.w,rotation.z*rotation.w)) * modelViewMatrix;
    return res;
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

void TransformState::setOrthoProjection(double left, double right, double bottom, double top, double nearVal, double farVal)
{
    // Note: Create an orthographic matrix from input values
    if (left == right) {
        right = left+1.0; // TODO: should throw an error here
    }
    if (bottom == top) {
        bottom = top+1.0; // TODO: should throw an error here
    }
    if (nearVal == farVal) {
        nearVal = farVal+1.0; // TODO: should throw an error here
    }

    double xx = 2.0/(right-left);
    double yy = 2.0/(top-bottom);
    double zz = -2.0/(farVal-nearVal);

    double tx = -(right+left)/(right-left);
    double ty = -(top+bottom)/(top-bottom);
    double tz = -(farVal+nearVal)/(farVal-nearVal);

    /*this->projMatrix = M44d(xx,0.0,0.0,tx,
                            0.0,yy,0.0,ty,
                            0.0,0.0,zz,tz,
                            0.0,0.0,0.0,1.0);*/

    this->projMatrix = M44d(xx,0.0,0.0,0.0,
                            0.0,yy,0.0,0.0,
                            0.0,0.0,zz,0.0,
                            tx,ty,tz,1.0);
}

void TransformState::load() const
{
#ifdef OPEN_GL_2
    glMatrixMode(GL_PROJECTION);
    glLoadMatrix(M44f(projMatrix));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrix(M44f(modelViewMatrix));
#endif
}


//------------------------------------------------------------------------------

/*void drawBoundingBox(const TransformState& transState,
                     const Imath::Box3d& bbox,
                     const Imath::C3f& col,
                     const GLuint& shaderProg)
{
    // Transform to box min for stability with large offsets
    TransformState trans2 = transState.translate(bbox.min);
    //Imath::Box3f box2(V3f(0), V3f(bbox.max - bbox.min));
    drawBoundingBox(trans2, box2, col, shaderProg);
}*/


void drawBoundingBox(const TransformState& transState,
                     const GLuint& bboxVertexArray,
                     const Imath::C3f& col,
                     const GLuint& shaderProg)
{
    //tfm::printfln("drawBoundingBox :: shaderProg: %i, vertexArray: %i", shaderProg, bboxVertexArray);

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1);

    // should already be bound ...
    //glUseProgram(shaderProg);

    transState.setUniforms(shaderProg);
    GLint colorLoc = glGetUniformLocation(shaderProg, "color");
    //assert(colorLoc >= 0); //this can easily happen, if you don't USE "color" in the shader due to optimization
    glUniform4f(colorLoc, col.x, col.y, col.z, 1.0); // , col.a

    glBindVertexArray(bboxVertexArray);
    //glDrawArrays( GL_LINES, 0, 8 );
    glDrawElements(GL_LINES, 3*8, GL_UNSIGNED_BYTE, 0);

    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}


void drawSphere(const TransformState& transState,
                const Imath::V3d& centre, double radius,
                GLuint shaderProg, const Imath::C4f& color,
                int nphi, int ntheta)
{
    // Transform centre into camera coordinate system, and remove that part of
    // the transform from the transform passed to the shader.
    V3d c2 = centre * transState.modelViewMatrix;
    TransformState newTrans = transState;
    newTrans.modelViewMatrix = M44d();

    // Generate vertices with usual spherical coordinate parameterization
    std::unique_ptr<GLfloat[]> verts(new GLfloat[3*ntheta*nphi]);
    for (int itheta = 0; itheta < ntheta; ++itheta)
    {
        double theta = M_PI*itheta/(ntheta-1);
        for (int iphi = 0; iphi < nphi; ++iphi)
        {
            double phi = 2*M_PI*iphi/nphi;
            int idx = nphi*itheta + iphi;
            double vx = cos(phi)*sin(theta);
            double vy = sin(phi)*sin(theta);
            double vz = -cos(theta);
            verts[3*idx]   = float(c2.x + radius*vx);
            verts[3*idx+1] = float(c2.y + radius*vy);
            verts[3*idx+2] = float(c2.z + radius*vz);
        }
    }
    std::vector<GLushort> triInds;
    for (int itheta = 0; itheta < ntheta-1; ++itheta)
    {
        for (int iphi = 0; iphi < nphi; ++iphi)
        {
            int i1 = nphi*itheta     + iphi;
            int i2 = nphi*(itheta+1) + iphi;
            int i3 = nphi*(itheta+1) + (iphi+1)%nphi;
            int i4 = nphi*itheta     + (iphi+1)%nphi;
            // Represent each quadrialteral patch from the parameterization
            // using two triangles
            triInds.push_back(i1); triInds.push_back(i2); triInds.push_back(i3);
            triInds.push_back(i1); triInds.push_back(i3); triInds.push_back(i4);
        }
    }

    // Draw computed sphere mesh.
    glUseProgram(shaderProg);
    newTrans.setUniforms(shaderProg);
    GLint colorLoc = glGetUniformLocation(shaderProg, "color");
    assert(colorLoc >= 0);
    glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
    GLint positionLoc = glGetAttribLocation(shaderProg, "position");
    assert(positionLoc >= 0);
    glVertexAttribPointer(positionLoc, 3, GL_FLOAT, GL_FALSE, 0, verts.get());
    glEnableVertexAttribArray(positionLoc);
    glDrawElements(GL_TRIANGLES, (GLint)triInds.size(),
                   GL_UNSIGNED_SHORT, triInds.data());
    glDisableVertexAttribArray(positionLoc);
#ifdef OPEN_GL_2
    glUseProgram(0);
#endif
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

