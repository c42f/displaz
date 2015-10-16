// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "geometry.h"
#include "hcloudview.h"
#include "mesh.h"
#include "ply_io.h"
#include "pointarray.h"

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
    m_bbox()//,
    //m_bboxVertexArray(0)
{ }


std::shared_ptr<Geometry> Geometry::create(QString fileName)
{
    if (fileName.endsWith(".ply") && plyHasMesh(fileName))
        return std::shared_ptr<Geometry>(new TriMesh());
    else if(fileName.endsWith(".hcloud"))
        return std::shared_ptr<Geometry>(new HCloudView());
    else
        return std::shared_ptr<Geometry>(new PointArray());
}


bool Geometry::reloadFile(size_t maxVertexCount)
{
    return loadFile(m_fileName, maxVertexCount);
}


void Geometry::initializeGL()
{
    tfm::printfln("Geometry::initializeGL");
    tfm::printfln("Geometry::bbox:\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n%f,%f,%f\n",
                  (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.min.z,
                  (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.min.z,
                  (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.min.z,
                  (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.min.z,
                  (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.max.z,
                  (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.max.z,
                  (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.max.z,
                  (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.max.z
    );

    GLfloat verts[] = {
            (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.min.z,
            (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.min.z,
            (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.min.z,
            (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.min.z,
            (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.max.z,
            (GLfloat)m_bbox.min.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.max.z,
            (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.max.y, (GLfloat)m_bbox.max.z,
            (GLfloat)m_bbox.max.x, (GLfloat)m_bbox.min.y, (GLfloat)m_bbox.max.z
    };
    unsigned char inds[] = {
            // rows: bottom, sides, top
            0,1, 1,2, 2,3, 3,0,
            0,4, 1,5, 2,6, 3,7,
            4,5, 5,6, 6,7, 7,4
    };


    // create VBA VBO for rendering ...
    glGenVertexArrays(1, &m_bboxVertexArray);
    glBindVertexArray(m_bboxVertexArray);

    GLuint geomVertexArray;
    glGenBuffers(1, &geomVertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, geomVertexArray);
    glBufferData(GL_ARRAY_BUFFER, 3*8*sizeof(float), verts, GL_STATIC_DRAW);

    //GLuint geomElementBuffer;
    //glGenBuffers(1, &geomElementBuffer);
    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geomElementBuffer);
    //glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2*4*3*sizeof(char), inds, GL_STATIC_DRAW);

    //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
