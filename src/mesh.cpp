// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "mesh.h"

#include <memory>

#include "glutil.h"
#include <QOpenGLShaderProgram>

#include "tinyformat.h"
#include "rply/rply.h"

#include "ply_io.h"
#include "PolygonBuilder.h"
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


/// Context to be passed to ply read callbacks
struct PlyLoadInfo
{
    long nvertices;
    std::vector<float> verts;
    double offset[3]; /// Global offset for verts array
    std::vector<float> colors;

    std::vector<GLuint> triangles;
    std::vector<GLuint> edges;
    long prevEdgeIndex;
    PolygonBuilder currPoly;
    std::vector<PolygonBuilder> deferredPolys;

    PlyLoadInfo()
        : nvertices(0),
        prevEdgeIndex(-1)
    {
        offset[0] = offset[1] = offset[2] = 0;
    }

    // Perform any computations required to clean things up after all ply
    // elements have been loaded.
    void postprocess()
    {
        for (size_t i = 0; i < deferredPolys.size(); ++i)
            deferredPolys[i].triangulate(verts, triangles);
    }
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


static int face_cb(p_ply_argument argument)
{
    long length;
    long index;
    ply_get_argument_property(argument, NULL, &length, &index);
    void* pinfo = 0;
    long indexType = 0;
    ply_get_argument_user_data(argument, &pinfo, &indexType);
    PlyLoadInfo& info = *(PlyLoadInfo*)pinfo;
    long vertexIndex = ply_get_argument_value(argument);
    if (info.currPoly.addIndex(indexType, length, index, vertexIndex))
    {
        // If the vertex array has been read, we can immediately triangulate.
        // If not, need to defer until later.
        if (info.verts.empty())
            info.deferredPolys.push_back(info.currPoly);
        else
            info.currPoly.triangulate(info.verts, info.triangles);
        info.currPoly.reset();
    }
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
    long vertexIndex = ply_get_argument_value(argument);
    if (vertexIndex < 0 || vertexIndex >= info.nvertices)
    {
        g_logger.warning("Vertex index %d outside of valid range [0,%d] - ignoring edge",
                         vertexIndex, info.nvertices-1);
        info.prevEdgeIndex = -1;
        return 1;
    }
    // Duplicate indices within a single edge chain so that we can pass them to
    // OpenGL as GL_LINES (or could use GL_LINE_STRIP?)
    if (info.prevEdgeIndex != -1)
    {
        info.edges.push_back((GLuint)info.prevEdgeIndex);
        info.edges.push_back((GLuint)vertexIndex);
    }
    info.prevEdgeIndex = (index < length-1) ? vertexIndex : -1;
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
    info.nvertices = nvertices;
    info.currPoly.setVertexCount(nvertices);
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

    long nfaces = 0;
    long nedges = 0;

    // Attach callbacks for faces.  Try both face.vertex_index and
    // face.vertex_indices since there doesn't seem to be a real standard.
    nfaces += ply_set_read_cb(ply.get(), "face", "vertex_index",
                              face_cb, &info, PolygonBuilder::OuterRingInds);
    nfaces += ply_set_read_cb(ply.get(), "face", "vertex_indices",
                              face_cb, &info, PolygonBuilder::OuterRingInds);

    // Attach callbacks for polygons with holes.
    // This isn't a standard at all, it's something I just made up :-/
    long npolygons = 0;
    npolygons += ply_set_read_cb(ply.get(), "polygon", "vertex_index",
                                 face_cb, &info, PolygonBuilder::OuterRingInds);
    npolygons += ply_set_read_cb(ply.get(), "polygon", "outer_vertex_index",
                                 face_cb, &info, PolygonBuilder::OuterRingInds); // DEPRECATED
    nfaces += npolygons;
    if (npolygons > 0)
    {
        // Holes
        if (ply_set_read_cb(ply.get(), "polygon", "inner_vertex_counts",
                            face_cb, &info, PolygonBuilder::InnerRingSizes))
        {
            if (!ply_set_read_cb(ply.get(), "polygon", "inner_vertex_index",
                                 face_cb, &info, PolygonBuilder::InnerRingInds))
            {
                g_logger.error("Found ply property polygon.inner_vertex_counts "
                               "without polygon.inner_vertex_index");
                return false;
            }
            info.currPoly.setPropertiesAvailable(PolygonBuilder::OuterRingInds |
                                                 PolygonBuilder::InnerRingSizes |
                                                 PolygonBuilder::InnerRingInds);
        }
    }

    // Attach callbacks for edges.  AFAIK this isn't really a standard, I'm
    // just copying the semi-standard people use for faces.
    nedges += ply_set_read_cb(ply.get(), "edge", "vertex_index",   edge_cb, &info, 0);
    nedges += ply_set_read_cb(ply.get(), "edge", "vertex_indices", edge_cb, &info, 0);

    if (nedges <= 0 && nfaces <= 0)
    {
        g_logger.error("Expected more than zero edges or faces in ply file");
        return false;
    }

    if (!ply_read(ply.get()))
    {
        g_logger.error("Error reading ply file data section");
        return false;
    }
    info.postprocess();
    if (info.colors.size() != info.verts.size())
        info.colors.clear();

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
    m_triangles.swap(info.triangles);
    m_edges.swap(info.edges);
    //makeSmoothNormals(m_normals, m_verts, m_triangles);
    //makeEdges(m_edges, m_triangles);
    return true;
}

void TriMesh::drawFaces(QOpenGLShaderProgram& prog,
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
    glDrawElements(GL_TRIANGLES, (GLsizei)m_triangles.size(),
                   GL_UNSIGNED_INT, &m_triangles[0]);
    prog.disableAttributeArray("color");
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("normal");
}


void TriMesh::drawEdges(QOpenGLShaderProgram& prog,
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
                         const EllipticalDist& distFunc,
                         V3d& pickedVertex,
                         double* distance,
                         std::string* info) const
{
    if (m_verts.empty())
        return false;

    size_t idx = distFunc.findNearest(offset(), (V3f*)&m_verts[0],
                                      m_verts.size()/3, distance);

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
                                const std::vector<unsigned int>& triangles)
{
    normals.resize(verts.size());
    const V3f* P = (const V3f*)&verts[0];
    V3f* N = (V3f*)&normals[0];
    for (size_t i = 0; i < triangles.size(); i += 3)
    {
        V3f P1 = P[triangles[i]];
        V3f P2 = P[triangles[i+1]];
        V3f P3 = P[triangles[i+2]];
        V3f Ni = ((P1 - P2).cross(P1 - P3)).normalized();
        N[triangles[i]] += Ni;
        N[triangles[i+1]] += Ni;
        N[triangles[i+2]] += Ni;
    }
    for (size_t i = 0; i < normals.size()/3; ++i)
    {
        if (N[i].length2() > 0)
            N[i].normalize();
    }
}


/// Figure out a unique set of edges for the given faces
void TriMesh::makeEdges(std::vector<unsigned int>& edges,
                        const std::vector<unsigned int>& triangles)
{
    std::vector<std::pair<GLuint,GLuint> > edgePairs;
    for (size_t i = 0; i < triangles.size(); i += 3)
    {
        for (int j = 0; j < 3; ++j)
        {
            GLuint i1 = triangles[i + j];
            GLuint i2 = triangles[i + (j+1)%3];
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

