// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_POINTARRAY_H_INCLUDED
#define DISPLAZ_POINTARRAY_H_INCLUDED

#include <cassert>
#include <cstdint>
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

        virtual DrawCount drawPoints(QGLShaderProgram& prog,
                                    const TransformState& transState,
                                    double quality, bool incrementalDraw) const;

        virtual size_t pointCount() const { return m_npoints; }

        virtual void estimateCost(const TransformState& transState,
                                  bool incrementalDraw, const double* qualities,
                                  DrawCount* drawCounts, int numEstimates) const;

        virtual bool pickVertex(const V3d& cameraPos,
                                const V3d& rayOrigin,
                                const V3d& rayDirection,
                                const double longitudinalScale,
                                V3d& pickedVertex,
                                double* distance = 0,
                                std::string* info = 0) const;

        /// Draw a representation of the point hierarchy.
        ///
        /// Probably only useful for debugging.
        void drawTree(const TransformState& transState) const;

    private:
        bool loadLas(QString fileName, size_t maxPointCount,
                     std::vector<GeomField>& fields, V3d& offset,
                     size_t& npoints, uint64_t& totalPoints,
                     Imath::Box3d& bbox, V3d& centroid);

        bool loadText(QString fileName, size_t maxPointCount,
                      std::vector<GeomField>& fields, V3d& offset,
                      size_t& npoints, uint64_t& totalPoints,
                      Imath::Box3d& bbox, V3d& centroid);

        bool loadPly(QString fileName, size_t maxPointCount,
                     std::vector<GeomField>& fields, V3d& offset,
                     size_t& npoints, uint64_t& totalPoints,
                     Imath::Box3d& bbox, V3d& centroid);

        friend struct ProgressFunc;

        /// Total number of loaded points
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
