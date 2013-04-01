#include "mesh.h"

#include <memory>

#include <QtOpenGL/qgl.h>
#include <QtOpenGL/QGLShaderProgram>

#include "tinyformat.h"
#include "rply/rply.h"

struct MeshInfo
{
    std::vector<float>* verts;
    std::vector<GLuint>* faces;
    double offset[3];
};

static int vertex_cb(p_ply_argument argument)
{
    void* pinfo = 0;
    ply_get_argument_user_data(argument, &pinfo, NULL);
    MeshInfo& info = *((MeshInfo*)pinfo);
    double v = ply_get_argument_value(argument);
    if (info.verts->size() < 3)
    {
        // First vertex is used for the constant offset
        info.offset[info.verts->size()] = v;
    }
    v -= info.offset[info.verts->size() % 3];
    info.verts->push_back((float)v);
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
    void* pinfo = 0;
    ply_get_argument_user_data(argument, &pinfo, NULL);
    ((MeshInfo*)pinfo)->faces->push_back(
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
    long ntriangles = ply_set_read_cb(ply.get(), "face", "vertex_index", face_cb, &info, 0);
    m_verts.reserve(3*nvertices);
    m_faces.reserve(3*ntriangles);
    if (!ply_read(ply.get()))
        return false;
    m_offset = V3d(info.offset[0], info.offset[1], info.offset[2]);
    makeSmoothNormals(m_normals, m_verts, m_faces);
    makeEdges(m_edges, m_faces);
    return true;
}


void TriMesh::drawFaces(QGLShaderProgram& prog)
{
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("normal");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    prog.setAttributeArray("normal", GL_FLOAT, &m_normals[0], 3);
    glDrawElements(GL_TRIANGLES, m_faces.size(),
                   GL_UNSIGNED_INT, &m_faces[0]);
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("normal");
}


void TriMesh::drawEdges(QGLShaderProgram& prog)
{
    prog.enableAttributeArray("position");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    glDrawElements(GL_LINES, m_edges.size(),
                   GL_UNSIGNED_INT, &m_edges[0]);
    prog.disableAttributeArray("position");
}


/// Compute smooth normals by averaging normals on connected faces
void TriMesh::makeSmoothNormals(std::vector<float>& normals,
                                const std::vector<float>& verts,
                                const std::vector<unsigned int>& faces)
{
    normals.resize(verts.size());
    const V3f* P = (const V3f*)&verts[0];
    V3f* N = (V3f*)&normals[0];
    for (size_t i = 0; i < faces.size(); i += 3)
    {
        V3f P1 = P[faces[i]];
        V3f P2 = P[faces[i+1]];
        V3f P3 = P[faces[i+2]];
        V3f Ni = ((P1 - P2).cross(P1 - P3)).normalized();
        N[faces[i]] += Ni;
        N[faces[i+1]] += Ni;
        N[faces[i+2]] += Ni;
    }
    for (size_t i = 0; i < normals.size()/3; ++i)
    {
        if (N[i].length2() > 0)
            N[i].normalize();
    }
}


/// Figure out a unique set of edges for the given faces
void TriMesh::makeEdges(std::vector<unsigned int>& edges,
                        const std::vector<unsigned int>& faces)
{
    std::vector<std::pair<GLuint,GLuint> > edgePairs;
    for (size_t i = 0; i < faces.size(); i += 3)
    {
        for (int j = 0; j < 3; ++j)
        {
            GLuint i1 = faces[i + j];
            GLuint i2 = faces[i + (j+1)%3];
            if (i1 > i2)
                std::swap(i1,i2);
            edgePairs.push_back(std::make_pair(i1, i2));
        }
    }
    std::sort(edgePairs.begin(), edgePairs.end());
    edgePairs.erase(std::unique(edgePairs.begin(), edgePairs.end()), edgePairs.end());
    edges.resize(2*edgePairs.size());
    for (size_t i = 0; i < edgePairs.size(); ++i)
    {
        edges[2*i]     = edgePairs[i].first;
        edges[2*i + 1] = edgePairs[i].second;
    }
}

