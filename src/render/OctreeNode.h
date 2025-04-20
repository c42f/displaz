// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#pragma once

#include <memory>
#include <vector>

#include "util.h"
#include "glutil.h"
#include "GeomField.h"

//------------------------------------------------------------------------------
/// Functor to compute octree child node index with respect to some given split
/// point
struct OctreeChildIdx
{
    const V3f* m_P;
    V3f m_center;
    int operator()(size_t i)
    {
        V3f p = m_P[i];
        return 4*(p.z >= m_center.z) +
               2*(p.y >= m_center.y) +
                 (p.x >= m_center.x);
    }

    OctreeChildIdx(const V3f* P, const V3f& center) : m_P(P), m_center(center) {}
};


struct OctreeNode
{
    OctreeNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    size_t beginIndex;       ///< Begin index of points in this node
    size_t endIndex;         ///< End index of points in this node
    mutable size_t nextBeginIndex; ///< Next index for incremental rendering
    Imath::Box3f bbox;       ///< Actual bounding box of points in node
    V3f center;              ///< center of the node
    float halfWidth;         ///< Half the axis-aligned width of the node.

    OctreeNode(const V3f& center, float halfWidth)
        : beginIndex(0), endIndex(0), nextBeginIndex(0),
        center(center), halfWidth(halfWidth)
    {
        std::fill(children, children + 8, nullptr);
    }

    ~OctreeNode()
    {
        std::for_each(children, children + 8, [](auto v) { delete v; });
    }

    size_t findNearest(const EllipticalDist& distFunc,
                       const V3d& offset, const V3f* p,
                       double& dist) const
    {
         return beginIndex + distFunc.findNearest(offset, p + beginIndex,
                                                  endIndex - beginIndex,
                                                  &dist);
    }

    size_t size() const { return endIndex - beginIndex; }

    bool isLeaf() const { return beginIndex != endIndex; }

    /// Estimate cost of drawing a single leaf node with to given camera
    /// position, quality, and incremental settings.
    ///
    /// Returns estimate of primitive draw count and whether there's anything
    /// more to draw.
    DrawCount drawCount(const V3f& relCamera,
                        double quality, bool incrementalDraw) const
    {
        assert(isLeaf());
        const double drawAllDist = 100;
        double dist = (this->bbox.center() - relCamera).length();
        double diagRadius = this->bbox.size().length()/2;
        // Subtract bucket diagonal dist, since we really want an approx
        // distance to closest point in the bucket, rather than dist to center.
        dist = std::max(10.0, dist - diagRadius);
        double desiredFraction = std::min(1.0, quality*pow(drawAllDist/dist, 2));
        size_t chunkSize = (size_t)ceil(this->size()*desiredFraction);
        DrawCount drawCount;
        drawCount.numVertices = chunkSize;
        if (incrementalDraw)
        {
            drawCount.numVertices = (this->nextBeginIndex >= this->endIndex) ? 0 :
                std::min(chunkSize, this->endIndex - this->nextBeginIndex);
        }
        drawCount.moreToDraw = this->nextBeginIndex < this->endIndex;
        return drawCount;
    }
};


struct ProgressFunc
{
    PointArray& points;
    size_t totProcessed;

    ProgressFunc(PointArray& points) : points(points), totProcessed(0) {}

    void operator()(size_t additionalProcessed)
    {
        totProcessed += additionalProcessed;
        emit points.loadProgress(int(100*totProcessed/points.pointCount()));
    }
};

/// Create an octree over the given set of points with position P
///
/// The points for consideration in the current node are the set
/// P[inds[beginIndex..endIndex]]; the tree building process sorts the inds
/// array in place so that points for the output leaf nodes are held in
/// the range P[inds[node.beginIndex, node.endIndex)]].  center is the central
/// split point for splitting children of the current node; radius is the
/// current node radius measured along one of the axes.
OctreeNode* makeTree(int depth, size_t* inds,
                     size_t beginIndex, size_t endIndex,
                     const V3f* P, const V3f& center,
                     float halfWidth, ProgressFunc& progressFunc)
{
    OctreeNode* node = new OctreeNode(center, halfWidth);
    const size_t pointsPerNode = 100000;
    // Limit max depth of tree to prevent infinite recursion when
    // greater than pointsPerNode points lie at the same position in
    // space.  floats effectively have 24 bit of precision in the
    // mantissa, so there's never any point splitting more than 24 times.
    const int maxDepth = 24;
    size_t* beginPtr = inds + beginIndex;
    size_t* endPtr = inds + endIndex;
    if (endIndex - beginIndex <= pointsPerNode || depth >= maxDepth)
    {
        static std::random_device rd;
        static std::mt19937 g(rd());
        std::shuffle(beginPtr, endPtr, g);

        // Leaf node: set up indices into point list 
        for (size_t i = beginIndex; i < endIndex; ++i)
            node->bbox.extendBy(P[inds[i]]);
        node->beginIndex = beginIndex;
        node->endIndex = endIndex;
        progressFunc(endIndex - beginIndex);
        return node;
    }
    // Partition points into the 8 child nodes
    size_t* childRanges[9] = {0};
    multi_partition(beginPtr, endPtr, OctreeChildIdx(P, center), &childRanges[1], 8);
    childRanges[0] = beginPtr;
    // Recursively generate child nodes
    float h = halfWidth/2;
    for (int i = 0; i < 8; ++i)
    {
        size_t childBeginIndex = childRanges[i]   - inds;
        size_t childEndIndex   = childRanges[i+1] - inds;
        if (childEndIndex == childBeginIndex)
            continue;
        V3f c = center + V3f((i     % 2 == 0) ? -h : h,
                             ((i/2) % 2 == 0) ? -h : h,
                             ((i/4) % 2 == 0) ? -h : h);
        node->children[i] = makeTree(depth+1, inds, childBeginIndex,
                                     childEndIndex, P, c, h, progressFunc);
        node->bbox.extendBy(node->children[i]->bbox);
    }
    return node;
}
