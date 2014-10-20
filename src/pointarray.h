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

#ifndef DISPLAZ_POINTARRAY_H_INCLUDED
#define DISPLAZ_POINTARRAY_H_INCLUDED

#include <cassert>
#include <memory>
#include <vector>

#include "geometry.h"
#include "typespec.h"
#include "geomfield.h"

class QGLShaderProgram;

struct OctreeNode;
struct TransformState;

//------------------------------------------------------------------------------
/// Container for points to be displayed in the View3D interface
class PointArray : public Geometry
{
    Q_OBJECT

    public:
        PointArray();
        ~PointArray();

        // Overridden Geometry functions
        virtual bool loadFile(QString fileName, size_t maxVertexCount);

        virtual size_t drawPoints(QGLShaderProgram& prog, const V3d& cameraPos,
                                  double quality, bool incrementalDraw) const;
        virtual size_t pointCount() const { return m_npoints; }
        virtual size_t simplifiedPointCount(const V3d& cameraPos, bool incrementalDraw) const;

        virtual V3d pickVertex(const V3d& cameraPos,
                               const V3d& rayOrigin, const V3d& rayDirection,
                               double longitudinalScale, double* distance = 0) const;

        /// Draw a representation of the point hierarchy.
        ///
        /// Probably only useful for debugging.
        void drawTree(const TransformState& transState) const;

    private:
        bool loadLas(QString fileName, size_t maxPointCount,
                     std::vector<GeomField>& fields, V3d& offset,
                     size_t& npoints, size_t& totPoints,
                     Imath::Box3d& bbox, V3d& centroid);

        bool loadText(QString fileName, size_t maxPointCount,
                      std::vector<GeomField>& fields, V3d& offset,
                      size_t& npoints, size_t& totPoints,
                      Imath::Box3d& bbox, V3d& centroid);

        bool loadPly(QString fileName, size_t maxPointCount,
                     std::vector<GeomField>& fields, V3d& offset,
                     size_t& npoints, size_t& totPoints,
                     Imath::Box3d& bbox, V3d& centroid);

        friend struct ProgressFunc;

        /// Total number of points
        size_t m_npoints;
        /// Spatial hierarchy
        std::unique_ptr<OctreeNode> m_rootNode;
        /// Point data field storage
        std::vector<GeomField> m_fields;
        /// A position field is required.  Alias for convenience:
        int m_positionFieldIdx;
        V3f* m_P;
};


#endif // DISPLAZ_POINTARRAY_H_INCLUDED
