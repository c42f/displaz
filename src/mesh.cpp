// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "mesh.h"

#include <memory>

#include "glutil.h"
#include <QtOpenGL/QGLShaderProgram>

#include "tinyformat.h"
#include "rply/rply.h"
#include "polypartition/polypartition.h"

#include "ply_io.h"
#include "qtlogger.h"


//------------------------------------------------------------------------------
// utils
static V3d getCentroid(const V3d& offset, const std::vector<float>& vertices)
{
    V3d posSum(0);
    for (size_t i = 0; i < vertices.size(); i+=3)
        posSum += V3d(vertices[i], vertices[i+1], vertices[i+2]);
    if (vertices.size() > 0)
        posSum = 1.0/vertices.size() * posSum;
    return posSum + offset;
}

static Imath::Box3d getBoundingBox(const V3d& offset, const std::vector<float>& vertices)
{
    Imath::Box3d bbox;
    for (size_t i = 0; i < vertices.size(); i+=3)
        bbox.extendBy(V3d(vertices[i], vertices[i+1], vertices[i+2]));
    if (!bbox.isEmpty())
    {
        bbox.min += offset;
        bbox.max += offset;
    }
    return bbox;
}

//------------------------------------------------------------------------------
// Stuff to load .ply files

struct PlyLoadInfo
{
    std::vector<unsigned int> tmpPolygon;
    std::vector<float> verts;
    std::vector<float> colors;
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


static int color_cb(p_ply_argument argument)
{
    void* pinfo = 0;
    ply_get_argument_user_data(argument, &pinfo, NULL);
    PlyLoadInfo& info = *((PlyLoadInfo*)pinfo);
    double v = ply_get_argument_value(argument);
    info.colors.push_back((float)v);
    return 1;
}


// Triangulate a polygon
//
// verts - 3D vertex positions
// inds  - indices of polygon vertices in `verts`; verts[3*inds[i]] is the x
//         component of the ith polygon vertex.
// triangleInds - indices of triangles in output array
static void triangulatePolygon(const std::vector<float>& verts,
                               const std::vector<GLuint>& inds,
                               std::vector<GLuint>& triangleInds)
{
    // Figure out which dimension the bounding box is smallest along, and
    // discard it to reduce dimensionality to 2D.
    Imath::Box3d bbox;
    for (size_t i = 0; i < inds.size(); ++i)
        bbox.extendBy(V3d(verts[3*inds[i]], verts[3*inds[i]+1], verts[3*inds[i]+2]));
    V3d diag = bbox.size();
    int xind = 0;
    int yind = 1;
    if (diag.z > std::min(diag.x, diag.y))
    {
        if (diag.x < diag.y)
            xind = 2;
        else
            yind = 2;
    }
    // Polypartition data structures
    TPPLPoly poly;
    poly.Init(inds.size());
    for (size_t i = 0; i < inds.size(); ++i)
    {
        poly[i].x = verts[3*inds[i]+xind];
        poly[i].y = verts[3*inds[i]+yind];
        poly[i].id = inds[i];
    }
    std::list<TPPLPoly> triangles;
    TPPLPartition polypartition;
    if (poly.GetOrientation() != TPPL_CCW)
        poly.Invert();
    // FIXME: Use Triangulate_MONO, after figuring out why it's not always
    // working.
    if (!polypartition.Triangulate_EC(&poly, &triangles))
    {
        g_logger.warning("Ignoring polygon which couldn't be triangulated.");
        return;
    }
    for (auto tri = triangles.begin(); tri != triangles.end(); ++tri)
    {
        triangleInds.push_back((*tri)[0].id);
        triangleInds.push_back((*tri)[1].id);
        triangleInds.push_back((*tri)[2].id);
    }
}

static int face_cb(p_ply_argument argument)
{
    long length;
    long index;
    ply_get_argument_property(argument, NULL, &length, &index);
    void* pinfo = 0;
    long isList = 0;
    ply_get_argument_user_data(argument, &pinfo, &isList);
    PlyLoadInfo& loadInfo = *(PlyLoadInfo*)pinfo;
    if (index < 0) // length argument
    {
        if (length != 3)
            loadInfo.tmpPolygon.resize(length);
        return 1;
    }
    if (isList && length != 3)
    {
        loadInfo.tmpPolygon[index] = (int)ply_get_argument_value(argument);
        if (index == length-1)
        {
            // FIXME: the verts array may not be initialized by the time we get
            // here!
            triangulatePolygon(loadInfo.verts, loadInfo.tmpPolygon,
                               loadInfo.faces);
        }
        return 1;
    }
    loadInfo.faces.push_back((unsigned int)ply_get_argument_value(argument));
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
    long isEdgeLoop = 0;
    ply_get_argument_user_data(argument, &pinfo, &isEdgeLoop);
    PlyLoadInfo& info = *((PlyLoadInfo*)pinfo);
    // Duplicate indices within a single edge chain so that we can pass them to
    // OpenGL as GL_LINES (or could use GL_LINE_STRIP?)
    if (index > 1)
        info.edges.push_back(info.edges.back());
    info.edges.push_back(
            (unsigned int)ply_get_argument_value(argument));
    if (isEdgeLoop && index == length-1)
    {
        // Add extra edge to close the loop
        int firstIdx = info.edges.end()[-2*(length-1)];
        info.edges.push_back(info.edges.back());
        info.edges.push_back(firstIdx);
    }
    return 1;
}

static int list_cb(p_ply_argument argument) {
    long length;
    long index;
    ply_get_argument_property(argument, NULL, &length, &index);
    if (index < 0) // ignore length argument
        return 1;
    void* plist = 0;
    ply_get_argument_user_data(argument, &plist, NULL);
    std::vector<unsigned int>& list = *((std::vector<unsigned int>*)plist);
    list.push_back((unsigned int)ply_get_argument_value(argument));
    return 1;
}


bool loadPlyFile(const QString& fileName,
                 PlyLoadInfo& info)
{
    // Read a triangulation from a .ply file
    std::unique_ptr<t_ply_, int(*)(p_ply)> ply(
            ply_open(fileName.toUtf8().constData(), logRplyError, 0, NULL), ply_close);
    if (!ply || !ply_read_header(ply.get()))
    {
        g_logger.error("Could not open ply or read header");
        return false;
    }
    long nvertices = ply_set_read_cb(ply.get(), "vertex", "x", vertex_cb, &info, 0);
    if (ply_set_read_cb(ply.get(), "vertex", "y", vertex_cb, &info, 1) != nvertices ||
        ply_set_read_cb(ply.get(), "vertex", "z", vertex_cb, &info, 2) != nvertices)
    {
        g_logger.error("Expected vertex properties (x,y,z) in ply file");
        return false;
    }
    info.verts.reserve(3*nvertices);
    long ncolors = ply_set_read_cb(ply.get(), "color", "r", color_cb, &info, 0);
    if (ncolors != 0)
    {
        ply_set_read_cb(ply.get(), "color", "g", color_cb, &info, 1);
        ply_set_read_cb(ply.get(), "color", "b", color_cb, &info, 2);
        info.colors.reserve(3*nvertices);
    }
    if (ncolors == 0)
    {
        ncolors = ply_set_read_cb(ply.get(), "vertex", "r", color_cb, &info, 0);
        if (ncolors != 0)
        {
            ply_set_read_cb(ply.get(), "vertex", "g", color_cb, &info, 1);
            ply_set_read_cb(ply.get(), "vertex", "b", color_cb, &info, 2);
            info.colors.reserve(3*nvertices);
        }
    }
    // Attempt to load attributes with names face/vertex_index or face/vertex_indices
    // There doesn't seem to be a real standard here...
    long nfaces = ply_set_read_cb(ply.get(), "face", "vertex_index", face_cb, &info, 1);
    if (nfaces == 0)
        nfaces = ply_set_read_cb(ply.get(), "face", "vertex_indices", face_cb, &info, 1);
    if (nfaces == 0)
    {
        nfaces = ply_set_read_cb(ply.get(), "triangle", "v1", face_cb, &info, 0);
        if (nfaces != 0 &&
            (ply_set_read_cb(ply.get(), "triangle", "v2", face_cb, &info, 0) != nfaces ||
             ply_set_read_cb(ply.get(), "triangle", "v3", face_cb, &info, 0) != nfaces))
        {
            g_logger.error("Expected triangle properties (v1,v2,v3) in ply file");
            return false;
        }
    }

    long nedges = ply_set_read_cb(ply.get(), "edge", "vertex_index", edge_cb, &info, 0);
    if (nedges == 0)
        nedges = ply_set_read_cb(ply.get(), "edge", "vertex_indices", edge_cb, &info, 0);

    // Support for specific Roames Ply format
    std::vector<unsigned int> innerPolygonVertexCount;
    std::vector<unsigned int> innerVertexIndices;
    if (nedges == 0)
    {
        nedges = ply_set_read_cb(ply.get(), "polygon", "outer_vertex_index", face_cb, &info, 1);
        nedges += ply_set_read_cb(ply.get(), "hullxy", "vertex_index", edge_cb, &info, 1);
        // FIXME: Deal with polygon holes correctly in triangulation.
        nedges += ply_set_read_cb(ply.get(), "polygon", "inner_polygon_vertex_counts", list_cb, &innerPolygonVertexCount, 0);
        ply_set_read_cb(ply.get(), "polygon", "inner_vertex_index", list_cb, &innerVertexIndices, 0);
    }

    if (nedges <= 0 && nfaces <= 0)
    {
        g_logger.error("Expected more than zero edges or faces in ply file");
        return false;
    }
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
    {
        g_logger.error("Error reading ply file data section");
        return false;
    }
    if (info.colors.size() != info.verts.size())
        info.colors.clear();

    // Reconstruct inner polygons
    for (size_t i = 0, j = 0; i < innerPolygonVertexCount.size(); ++i)
    {
        size_t count = innerPolygonVertexCount[i];
        for (size_t k = 0; k < count; ++k)
        {
            info.edges.push_back(innerVertexIndices[j + k]);
            info.edges.push_back(innerVertexIndices[j + (k+1) % count]);
        }
        j += count;
    }

    return true;
}


//------------------------------------------------------------------------------
// TriMesh implementation
bool TriMesh::loadFile(QString fileName, size_t /*maxVertexCount*/)
{
    // maxVertexCount is ignored - not sure there's anything useful we can do
    // to respect it when loading a mesh...
    PlyLoadInfo info;
    if (!loadPlyFile(fileName, info))
        return false;
    setFileName(fileName);
    V3d offset = V3d(info.offset[0], info.offset[1], info.offset[2]);
    setOffset(offset);
    setCentroid(getCentroid(offset, info.verts));
    setBoundingBox(getBoundingBox(offset, info.verts));
    m_verts.swap(info.verts);
    m_colors.swap(info.colors);
    m_faces.swap(info.faces);
    m_edges.swap(info.edges);
    //makeSmoothNormals(m_normals, m_verts, m_faces);
    //makeEdges(m_edges, m_faces);
    return true;
}

void TriMesh::drawFaces(QGLShaderProgram& prog,
                        const TransformState& transState) const
{
    transState.translate(offset()).setUniforms(prog.programId());
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("normal");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    prog.setAttributeArray("normal", GL_FLOAT, &m_normals[0], 3);
    if (m_colors.size() == m_verts.size())
    {
        prog.enableAttributeArray("color");
        prog.setAttributeArray("color", GL_FLOAT, &m_colors[0], 3);
    }
    else
        prog.setAttributeValue("color", GLfloat(1), GLfloat(1), GLfloat(1));
    glDrawElements(GL_TRIANGLES, (GLsizei)m_faces.size(),
                   GL_UNSIGNED_INT, &m_faces[0]);
    prog.disableAttributeArray("color");
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("normal");
}


void TriMesh::drawEdges(QGLShaderProgram& prog,
                        const TransformState& transState) const
{
    transState.translate(offset()).setUniforms(prog.programId());
    prog.enableAttributeArray("position");
    prog.setAttributeArray("position", GL_FLOAT, &m_verts[0], 3);
    if (m_colors.size() == m_verts.size())
    {
        prog.enableAttributeArray("color");
        prog.setAttributeArray("color", GL_FLOAT, &m_colors[0], 3);
    }
    else
        prog.setAttributeValue("color", GLfloat(1), GLfloat(1), GLfloat(1));
    glDrawElements(GL_LINES, (GLsizei)m_edges.size(),
                   GL_UNSIGNED_INT, &m_edges[0]);
    prog.disableAttributeArray("color");
    prog.disableAttributeArray("position");
}


void TriMesh::estimateCost(const TransformState& transState,
                           bool incrementalDraw, const double* qualities,
                           DrawCount* drawCounts, int numEstimates) const
{
    // FIXME - we need a way to incorporate meshes into the cost model, even
    // though simplifying them in a similar way to point clouds isn't really
    // possible.
}


bool TriMesh::pickVertex(const V3d& cameraPos,
                         const V3d& rayOrigin,
                         const V3d& rayDirection,
                         const double longitudinalScale,
                         V3d& pickedVertex,
                         double* distance,
                         std::string* info) const
{
    if (m_verts.empty())
        return false;

    size_t idx = closestPointToRay((V3f*)&m_verts[0], m_verts.size()/3,
                                   rayOrigin - offset(), rayDirection,
                                   longitudinalScale, distance);

    pickedVertex = V3d(m_verts[3*idx], m_verts[3*idx+1], m_verts[3*idx+2]) + offset();

    if (info)
    {
        *info = "";
        *info += tfm::format("  position = %.3f %.3f %.3f\n", pickedVertex.x, pickedVertex.y, pickedVertex.z);
        if (!m_colors.empty())
        {
            *info += tfm::format("  color = %.3f %.3f %.3f\n",
                                 m_colors[3*idx], m_colors[3*idx+1], m_colors[3*idx+2]);
        }
    }

    return true;
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

