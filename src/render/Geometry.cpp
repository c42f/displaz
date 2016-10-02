// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "Geometry.h"
#include "HCloudView.h"
#include "TriMesh.h"
#include "ply_io.h"
#include "PointArray.h"

#include <rply/rply.h>

//TMP DEBUG
#include "tinyformat.h"

/// Determine whether a ply file has mesh or line segment elements
///
/// (If not, assume it's a point cloud.)
static bool plyHasMesh(QString fileName)
{
    std::unique_ptr<t_ply_, int(*)(p_ply)> ply(
            ply_open(fileName.toUtf8().constData(), logRplyError, 0, NULL), ply_close);
    if (!ply || !ply_read_header(ply.get()))
        return false;
    for (p_ply_element elem = ply_get_next_element(ply.get(), NULL);
         elem != NULL; elem = ply_get_next_element(ply.get(), elem))
    {
        const char* name = 0;
        long ninstances = 0;
        if (!ply_get_element_info(elem, &name, &ninstances))
            continue;
        if (strcmp(name, "face") == 0 || strcmp(name, "edge") == 0 ||
            strcmp(name, "polygon") == 0)
        {
            return true;
        }
    }
    return false;
}


//------------------------------------------------------------------------------
Geometry::Geometry()
    : m_fileName(),
    m_offset(0,0,0),
    m_centroid(0,0,0),
    m_bbox(),
    m_VAO(),
    m_VBO(),
    m_Shaders()
{ }

Geometry::~Geometry()
{
    destroyBuffers();
}

std::shared_ptr<Geometry> Geometry::create(QString fileName)
{
    if (fileName.endsWith(".ply") && plyHasMesh(fileName))
        return std::shared_ptr<Geometry>(new TriMesh());
    else if(fileName.endsWith(".hcloud"))
        return std::shared_ptr<Geometry>(new HCloudView());
    else
        return std::shared_ptr<Geometry>(new PointArray());
}

void Geometry::initializeGL()
{
    destroyBuffers();
}

void Geometry::destroyBuffers()
{
    // destroy any previously created buffers (in case we are re-initializing this geometry)
    // this should avoid recreating more and more opengl buffers

    for (auto& it: m_VBO)
    {
        GLuint vbo = it.second;
        glDeleteBuffers(1, &vbo);
    }

    m_VBO.clear();

    for (auto& it: m_VAO)
    {
        GLuint vao = it.second;
        glDeleteVertexArrays(1, &vao);
    }

    m_VAO.clear();

}

const unsigned int Geometry::getVAO(const char * vertexArrayName) const
{
    // always call this from an active OpenGL context
    if(m_VAO.find(std::string(vertexArrayName)) != m_VAO.end())
    {
        return m_VAO.at(std::string(vertexArrayName));
    }
    tfm::printfln("Geometry :: vertexArrayObject was not found - %s", vertexArrayName);
    return 0;
}

const unsigned int Geometry::getVBO(const char * vertexBufferName) const
{
    // always call this from an active OpenGL context
    if(m_VBO.find(std::string(vertexBufferName)) != m_VBO.end())
    {
        return m_VBO.at(std::string(vertexBufferName));
    }
    tfm::printfln("Geometry :: vertexBufferObject was not found - %s", vertexBufferName);
    return 0;
}

const unsigned int Geometry::shaderId(const char * shaderName) const
{
    if(m_Shaders.find(shaderName) != m_Shaders.end())
    {
        return m_Shaders.at(shaderName);
    }
    tfm::printfln("Geometry :: shaderId not found - %s", shaderName);
    return 0;
}
