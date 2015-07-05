// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef POLYGON_BUILDER_H_INCLUDED
#define POLYGON_BUILDER_H_INCLUDED

#include <vector>

#include "glutil.h"

/// Temporary storage for polygon vertex indices during file read
class PolygonBuilder
{
    public:
        enum FacePropertyType
        {
            OuterRingInds  = 0x1,
            InnerRingSizes = 0x2,
            InnerRingInds  = 0x4,
        };

        PolygonBuilder();

        /// Set which ply properties are available
        void setPropertiesAvailable(int avail) { m_propsAvail = avail; }

        /// Set total number of vertices available
        void setVertexCount(long vertexCount) { m_vertexCount = vertexCount; }

        /// Add index to polygon definition.
        ///
        /// propType      - One of FacePropertyType
        /// plyListLength - Length of ply property list currently being read
        /// plyListIndex  - Index in current ply property list
        /// vertexIndex   - Index of vertex to be added
        ///
        /// Return true if the current polygon is complete.
        bool addIndex(long propType, long plyListLength,
                      long plyListIndex, long vertexIndex);

        /// Triangulate polygon, using vertex list `verts`, pushing new
        /// triangle faces into triangleInds
        void triangulate(const std::vector<float>& verts, std::vector<GLuint>& triangleInds);

        /// Reset polygon ready for reading next set of indices
        void reset();

    private:
        bool m_valid;
        long m_vertexCount;
        int m_propsAvail;
        int m_propsRead;
        std::vector<GLuint> m_outerRingInds;  // Vertex indices of outer ring
        std::vector<GLuint> m_innerRingSizes; // Number of vertices for each inner ring
        std::vector<GLuint> m_innerRingInds;  // Count of inner ring indices
};


#endif // POLYGON_BUILDER_H_INCLUDED
