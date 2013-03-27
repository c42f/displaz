#include "mesh.h"

#include <memory>

#include <QtOpenGL/qgl.h>
#include <QtOpenGL/QGLShaderProgram>

#include "tinyformat.h"
#include "rply/rply.h"

// FIXME
#ifdef _WIN32
#include <ImathVec.h>
#include <ImathBox.h>
#include <ImathColor.h>
#else
#include <OpenEXR/ImathVec.h>
#include <OpenEXR/ImathBox.h>
#include <OpenEXR/ImathColor.h>
#endif

using Imath::V3d;
using Imath::V3f;
using Imath::V2f;
using Imath::C3f;

struct MeshInfo
{
    std::vector<double>* verts;
    std::vector<GLuint>* faces;
};

static int vertex_cb(p_ply_argument argument)
{
    void* info = 0;
    ply_get_argument_user_data(argument, &info, NULL);
    ((MeshInfo*)info)->verts->push_back(ply_get_argument_value(argument));
    return 1;
}

static int face_cb(p_ply_argument argument)
{
    long length;
    long index;
    ply_get_argument_property(argument, NULL, &length, &index);
    if (length != 3)
    {
        // Not a triangle - ignore for now!
        tfm::printf("WARNING: Discarding non-triangular face\n");
        return 1;
    }
    if (index < 0) // ignore length argument
        return 1;
    void* info = 0;
    ply_get_argument_user_data(argument, &info, NULL);
    ((MeshInfo*)info)->faces->push_back(
            (unsigned int)ply_get_argument_value(argument));
    return 1;
}


bool TriMesh::readFile(const std::string& fileName)
{
    // Read a triangulation from a .ply file
    std::unique_ptr<t_ply_, int(&)(p_ply)> ply(
            ply_open(fileName.c_str(), NULL, 0, NULL), ply_close);
    if (!ply || !ply_read_header(ply.get()))
        return false;
    MeshInfo info;
    info.verts = &m_verts;
    info.faces = &m_faces;
    long nvertices = ply_set_read_cb(ply.get(), "vertex", "x", vertex_cb, &info, 0);
    if (ply_set_read_cb(ply.get(), "vertex", "y", vertex_cb, &info, 1) != nvertices ||
        ply_set_read_cb(ply.get(), "vertex", "z", vertex_cb, &info, 2) != nvertices)
        return false;
    long ntriangles = ply_set_read_cb(ply.get(), "face", "vertex_indices", face_cb, &info, 0);
    m_verts.reserve(3*nvertices);
    m_faces.reserve(3*ntriangles);
    if (!ply_read(ply.get()))
        return false;
    // Compute smooth normals
    std::vector<V3f> normals;
    m_normals.resize(m_verts.size());
    const V3d* P = (const V3d*)&m_verts[0];
    V3f* N = (V3f*)&m_normals[0];
    for (size_t i = 0; i < m_faces.size(); i += 3)
    {
        V3f P1 = P[m_faces[i]];
        V3f P2 = P[m_faces[i+1]];
        V3f P3 = P[m_faces[i+2]];
        V3f Ni = ((P1 - P2).cross(P1 - P3)).normalized();
        N[m_faces[i]] += Ni;
        N[m_faces[i+1]] += Ni;
        N[m_faces[i+2]] += Ni;
    }
    for (size_t i = 0; i < normals.size()/3; ++i)
    {
        if (N[i].length2() > 0)
            N[i].normalize();
    }
    return true;
}


void TriMesh::draw(QGLShaderProgram& prog)
{
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("normal");
    prog.setAttributeArray("position", GL_DOUBLE, &m_verts[0], 3);
    prog.setAttributeArray("normal", GL_FLOAT, &m_normals[0], 3);
    glDrawElements(GL_TRIANGLES, m_faces.size(),
                   GL_UNSIGNED_INT, &m_faces[0]);
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("normal");
}
