// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

#include "Geometry.h"
#include "typespec.h"
#include "GeomField.h"
#include "GeometryMutator.h"

class QOpenGLShaderProgram;

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

        virtual void mutate(std::shared_ptr<GeometryMutator> mutator);

        virtual void draw(const TransformState& transState, double quality) const;

        virtual void initializeGL();

        virtual DrawCount drawPoints(QOpenGLShaderProgram& prog,
                                    const TransformState& transState,
                                    double quality, bool incrementalDraw) const override;

        virtual size_t pointCount() const { return m_npoints; }

        virtual void estimateCost(const TransformState& transState,
                                  bool incrementalDraw, const double* qualities,
                                  DrawCount* drawCounts, int numEstimates) const;

        virtual bool pickVertex(const V3d& cameraPos,
                                const EllipticalDist& distFunc,
                                V3d& pickedVertex,
                                double* distance = 0,
                                std::string* info = 0) const;

        /// Draw a representation of the point hierarchy.
        ///
        /// Probably only useful for debugging.
        void drawTree(QOpenGLShaderProgram& prog, const TransformState& transState) const;

    private:
        bool loadLas(QString fileName, size_t maxPointCount,
                     std::vector<GeomField>& fields, V3d& offset,
                     size_t& npoints, uint64_t& totalPoints);

        bool loadText(QString fileName, size_t maxPointCount,
                      std::vector<GeomField>& fields, V3d& offset,
                      size_t& npoints, uint64_t& totalPoints);

        bool loadPly(QString fileName, size_t maxPointCount,
                     std::vector<GeomField>& fields, V3d& offset,
                     size_t& npoints, uint64_t& totalPoints);

        friend struct ProgressFunc;

        /// Total number of loaded points
        size_t m_npoints = 0;
        /// Spatial hierarchy
        std::unique_ptr<OctreeNode> m_rootNode;
        /// Point data field storage
        std::vector<GeomField> m_fields;
        /// A position field is required.  Alias for convenience:
        int m_positionFieldIdx = -1;
        V3f* m_P = nullptr;
        std::unique_ptr<uint32_t[]> m_inds;
};
