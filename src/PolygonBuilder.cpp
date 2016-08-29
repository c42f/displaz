// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "PolygonBuilder.h"

#include "polypartition/polypartition.h"
#include "QtLogger.h"

// Initialize polypartition TPPLPoly from a list of indices and vertices
//
// verts     - 3D polygon vertex vectors
// xind,yind - Indices of 3D vectors to extract as x and y coordinates for 2D
//             triangulation computation.
// inds,size - Array of indices into the `verts` list
// isHole    - Value for the hole flag, and to determine the orientation
static void initTPPLPoly(TPPLPoly& poly,
                         const std::vector<float>& verts,
                         int xind, int yind,
                         const GLuint* inds, int size,
                         bool isHole)
{
    // Check for explicitly closed polygons (last and first vertices equal) and
    // discard the last vertex in these cases.  This is a pretty stupid
    // convention, but the OGC have blessed it and now we've got a bunch of
    // geospatial formats (kml, WKT, GeoJSON) which require it.  Sigh.
    // http://gis.stackexchange.com/questions/10308/why-do-valid-polygons-repeat-the-same-start-and-end-point/10309#10309
    if (inds[0] == inds[size-1] ||
        (verts[3*inds[0]+0] == verts[3*inds[size-1]+0] &&
         verts[3*inds[0]+1] == verts[3*inds[size-1]+1] &&
         verts[3*inds[0]+2] == verts[3*inds[size-1]+2]))
    {
        g_logger.warning_limited("Ignoring duplicate final vertex in explicitly closed polygon");
        size -= 1;
    }
    // Copy into polypartition data structure
    poly.Init(size);
    for (int i = 0; i < size; ++i)
    {
        poly[i].x = verts[3*inds[i]+xind];
        poly[i].y = verts[3*inds[i]+yind];
        poly[i].id = inds[i];
    }
    int orientation = poly.GetOrientation();
    // Invert so that outer = ccw, holes = cw
    if ((orientation == TPPL_CW) ^ isHole)
        poly.Invert();
    poly.SetHole(isHole);
}


// Triangulate a polygon, returning vertex indices to the triangles
//
// verts - 3D vertex positions
// outerRingInds  - Indices of outer boundary vertices in `verts` array;
//                  the j'th component of the i'th polygon vertex is
//                  verts[3*outerRingInds[i]+j].
// innerRingSizes - Number of vertices in each polygon hole
// innerRingInds  - Indices of vertices for all holes; the first hole has
//                  indices innerRingInds[i] for i in [0, innerRingSize[0])
// triangleInds   - Indices of triangular pieces are appended to this array.
//
// Return false if polygon couldn't be triangulated.
static bool triangulatePolygon(const std::vector<float>& verts,
                               const std::vector<GLuint>& outerRingInds,
                               const std::vector<GLuint>& innerRingSizes,
                               const std::vector<GLuint>& innerRingInds,
                               std::vector<GLuint>& triangleInds)
{
    // Figure out which dimension the bounding box is smallest along, and
    // discard it to reduce dimensionality to 2D.
    Imath::Box3d bbox;
    for (size_t i = 0; i < outerRingInds.size(); ++i)
    {
        GLuint j = 3*outerRingInds[i];
        assert(j + 2 < verts.size());
        bbox.extendBy(V3d(verts[j], verts[j+1], verts[j+2]));
    }
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
    std::list<TPPLPoly> triangles;
    if (innerRingSizes.empty())
    {
        TPPLPoly poly;
        // Simple case - no holes
        //
        // TODO: Use Triangulate_MONO, after figuring out why it's not always
        // working?
        initTPPLPoly(poly, verts, xind, yind,
                     outerRingInds.data(), (int)outerRingInds.size(), false);
        TPPLPartition polypartition;
        if (!polypartition.Triangulate_EC(&poly, &triangles))
        {
            g_logger.warning("Ignoring polygon which couldn't be triangulated.");
            return false;
        }
    }
    else
    {
        // Set up outer polygon boundary and holes.
        std::list<TPPLPoly> inputPolys;
        inputPolys.resize(1 + innerRingSizes.size());
        auto polyIter = inputPolys.begin();
        initTPPLPoly(*polyIter, verts, xind, yind,
                     outerRingInds.data(), (int)outerRingInds.size(), false);
        ++polyIter;
        for (size_t i = 0, j = 0; i < innerRingSizes.size(); ++i, ++polyIter)
        {
            size_t count = innerRingSizes[i];
            if (j + count > innerRingInds.size())
            {
                g_logger.warning("Ignoring polygon with inner ring %d of %d too large",
                                 i, innerRingSizes.size());
                return false;
            }
            initTPPLPoly(*polyIter, verts, xind, yind,
                         innerRingInds.data() + j, (int)count, true);
            j += count;
        }
        // Triangulate
        std::list<TPPLPoly> noHolePolys;
        TPPLPartition polypartition;
        polypartition.RemoveHoles(&inputPolys, &noHolePolys);
        if (!polypartition.Triangulate_EC(&noHolePolys, &triangles))
        {
            g_logger.warning("Ignoring polygon which couldn't be triangulated.");
            return false;
        }
    }
    for (auto tri = triangles.begin(); tri != triangles.end(); ++tri)
    {
        triangleInds.push_back((*tri)[0].id);
        triangleInds.push_back((*tri)[1].id);
        triangleInds.push_back((*tri)[2].id);
    }
    return true;
}


//------------------------------------------------------------------------------

PolygonBuilder::PolygonBuilder()
    : m_valid(true),
    m_vertexCount(0),
    m_propsAvail(OuterRingInds),
    m_propsRead(0)
{ }


bool PolygonBuilder::addIndex(long propType, long plyListLength,
                              long plyListIndex, long vertexIndex)
{
    assert(propType & m_propsAvail);
    m_propsRead |= propType; // support any ply property order
    size_t currSize = 0;
    if (plyListLength != 0 && plyListIndex >= 0)
    {
        switch (propType)
        {
#           define ADD_IND(vec) vec.push_back(vertexIndex); currSize = vec.size();
            case OuterRingInds:  ADD_IND(m_outerRingInds);  break;
            case InnerRingSizes: ADD_IND(m_innerRingSizes); break;
            case InnerRingInds:  ADD_IND(m_innerRingInds);  break;
#           undef ADD_IND
            default: assert(0 && "Unexpected index type");
        }
        if (propType != InnerRingSizes &&
            (vertexIndex < 0 || vertexIndex >= m_vertexCount))
        {
            g_logger.warning("Vertex index %d outside of valid range [0,%d] - ignoring polygon",
                                vertexIndex, m_vertexCount-1);
            m_valid = false;
        }
    }
    return m_propsRead == m_propsAvail && (long)currSize == plyListLength;
}


void PolygonBuilder::triangulate(const std::vector<float>& verts,
                                 std::vector<GLuint>& triangleInds)
{
    if (!m_valid)
        return;
    if (m_outerRingInds.size() == 3 && m_innerRingSizes.empty())
    {
        // Already a triangle - nothing to do.
        for (int i = 0; i < 3; ++i)
            triangleInds.push_back(m_outerRingInds[i]);
        return;
    }
    triangulatePolygon(verts, m_outerRingInds, m_innerRingSizes,
                        m_innerRingInds, triangleInds);
}


void PolygonBuilder::reset()
{
    m_valid = true;
    m_propsRead = 0;
    m_outerRingInds.clear();
    m_innerRingSizes.clear();
    m_innerRingInds.clear();
}


