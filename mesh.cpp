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

#include "mesh.h"

#include <memory>

#include <QtOpenGL/qgl.h>
#include <QtOpenGL/QGLShaderProgram>

#include "tinyformat.h"
#include "rply/rply.h"


//------------------------------------------------------------------------------
// utils
V3d getCentroid(const V3d& offset, const std::vector<float>& vertices)
{
    V3d posSum(0);
    for (size_t i = 0; i < vertices.size(); i+=3)
        posSum += V3d(vertices[i], vertices[i+1], vertices[i+2]);
    if (vertices.size() > 0)
        posSum = (3.0/vertices.size())*posSum;
    return posSum + offset;
}


//------------------------------------------------------------------------------
// Stuff to load .ply files

struct PlyLoadInfo
{
    std::vector<float> verts;
    std::vector<GLuint> faces;
    std::vector<GLuint> edges;
    double offset[3];
};


static int vertex_cb(p_ply_argument argument)
{
    void* pinfo = 0;
    ply_get_argument_user_data(argument, &pinfo, NULL);
    PlyLoadInfo& info = *((PlyLoadInfo*)pinfo);
    double v = ply_get_argument_value(argument);
    if (info.verts.size() < 3)
    {
        // First vertex is used for the constant offset
        info.offset[info.verts.size()] = v;
    }
    v -= info.offset[info.verts.size() % 3];
    info.verts.push_back((float)v);
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
    ((PlyLoadInfo*)pinfo)->faces.push_back(
            (unsigned int)ply_get_argument_value(argument));
    return 1;
}


static int edge_cb(p_ply_argument argument)
{
    long length;
    long index;
    ply_get_argument_property(argument, NULL, &length, &index);
    if (index < 0) // ignore length argument
        return 1;
    void* pinfo = 0;
    ply_get_argument_user_data(argument, &pinfo, NULL);
    PlyLoadInfo& info = *((PlyLoadInfo*)pinfo);
    // Duplicate indices within a single edge chain so that we can pass them to
    // OpenGL as GL_LINES (or could use GL_LINE_STRIP?)
    if (index > 1)
        info.edges.push_back(info.edges.back());
    info.edges.push_back(
            (unsigned int)ply_get_argument_value(argument));
    return 1;
}


bool readPlyFile(const QString& fileName,
                 std::unique_ptr<TriMesh>& mesh,
                 std::unique_ptr<LineSegments>& lines)
{
    // Read a triangulation from a .ply file
    typedef int (*ply_close_t)(p_ply);
    std::unique_ptr<t_ply_, ply_close_t> ply(
            ply_open(fileName.toUtf8().constData(), NULL, 0, NULL), ply_close);
    if (!ply || !ply_read_header(ply.get()))
        return false;
    PlyLoadInfo info;
    long nvertices = ply_set_read_cb(ply.get(), "vertex", "x", vertex_cb, &info, 0);
    if (ply_set_read_cb(ply.get(), "vertex", "y", vertex_cb, &info, 1) != nvertices ||
        ply_set_read_cb(ply.get(), "vertex", "z", vertex_cb, &info, 2) != nvertices)
        return false;
    info.verts.reserve(3*nvertices);
    long nfaces = ply_set_read_cb(ply.get(), "face", "vertex_index", face_cb, &info, 0);
    long nedges = ply_set_read_cb(ply.get(), "edge", "vertex_index", edge_cb, &info, 0);
    if (nedges <= 0 && nfaces <= 0)
        return false;
    if (nfaces > 0)
    {
        // Ply file contains a mesh - load as triangle mesh
        info.faces.reserve(3*nfaces);
    }
    if (nedges > 0)
    {
        // Ply file contains a set of edges
        info.edges.reserve(2*nedges);
    }
    if (!ply_read(ply.get()))
        return false;
    V3d offset = V3d(info.offset[0], info.offset[1], info.offset[2]);
    if (info.faces.size() > 0)
        mesh.reset(new TriMesh(offset, info.verts, info.faces));
    if (info.edges.size() > 0)
        lines.reset(new LineSegments(offset, info.verts, info.edges));
    return true;
}


//------------------------------------------------------------------------------
// TriMesh implementation
TriMesh::TriMesh(const V3d& offset, const std::vector<float>& vertices,
                 const std::vector<unsigned int>& faces)
    : m_offset(offset),
    m_centroid(getCentroid(offset, vertices)),
    m_verts(vertices),
    m_faces(faces)
{
    makeSmoothNormals(m_normals, m_verts, m_faces);
    makeEdges(m_edges, m_faces);
}


void TriMesh::drawFaces(QGLShaderProgram& prog) const
{
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("normal");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    prog.setAttributeArray("normal", GL_FLOAT, &m_normals[0], 3);
    glDrawElements(GL_TRIANGLES, (GLsizei)m_faces.size(),
                   GL_UNSIGNED_INT, &m_faces[0]);
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("normal");
}


void TriMesh::drawEdges(QGLShaderProgram& prog) const
{
    prog.enableAttributeArray("position");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    glDrawElements(GL_LINES, (GLsizei)m_edges.size(),
                   GL_UNSIGNED_INT, &m_edges[0]);
    prog.disableAttributeArray("position");
}


size_t TriMesh::closestVertex(const V3d& rayOrigin, const V3f& rayDirection,
                              double longitudinalScale, double* distance) const
{
    return closestPointToRay((V3f*)&m_verts[0], m_verts.size()/3,
                             rayOrigin - m_offset, rayDirection,
                             longitudinalScale, distance);
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


//------------------------------------------------------------------------------
// LineSegments implementation

LineSegments::LineSegments(const V3d& offset, const std::vector<float>& vertices,
                           const std::vector<unsigned int>& edges)
    : m_offset(offset),
    m_centroid(getCentroid(offset, vertices)),
    m_verts(vertices),
    m_edges(edges)
{ }


void LineSegments::drawEdges(QGLShaderProgram& prog) const
{
    prog.enableAttributeArray("position");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    glDrawElements(GL_LINES, (GLsizei)m_edges.size(),
                   GL_UNSIGNED_INT, &m_edges[0]);
    prog.disableAttributeArray("position");
}


size_t LineSegments::closestVertex(const V3d& rayOrigin, const V3f& rayDirection,
                                   double longitudinalScale, double* distance) const
{
    return closestPointToRay((V3f*)&m_verts[0], m_verts.size()/3,
                             rayOrigin - m_offset, rayDirection,
                             longitudinalScale, distance);
}
