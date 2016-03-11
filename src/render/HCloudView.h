// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt


#ifndef DISPLAZ_HCLOUDVIEW_H_INCLUDED
#define DISPLAZ_HCLOUDVIEW_H_INCLUDED

#include <fstream>

#include "Geometry.h"
#include "hcloud.h"

struct HCloudNode;
class StreamPageCache;
class ShaderProgram;

/// Viewer for hcloud file format
///
/// HCloudView uses incremental loading of the LoD structure to avoid loading
/// the whole thing into memory at once.
class HCloudView : public Geometry
{
    Q_OBJECT
    public:
        HCloudView();

        ~HCloudView();

        virtual bool loadFile(QString fileName, size_t maxVertexCount);

        virtual void initializeGL();

        virtual void draw(const TransformState& transState, double quality) const;

        virtual size_t pointCount() const;

        virtual void estimateCost(const TransformState& transState,
                                  bool incrementalDraw, const double* qualities,
                                  DrawCount* drawCounts, int numEstimates) const;

        virtual bool pickVertex(const V3d& cameraPos,
                                const EllipticalDist& distFunc,
                                V3d& pickedVertex,
                                double* distance = 0,
                                std::string* info = 0) const;


    private:
        HCloudHeader m_header; // TODO: Put in HCloudInput class
        // TODO: Do we really want all this mutable state?
        // Should draw() be logically non-const?
        mutable uint64_t m_sizeBytes;
        mutable std::ifstream m_input;
        mutable std::unique_ptr<StreamPageCache> m_inputCache;
        std::unique_ptr<HCloudNode> m_rootNode;
        std::unique_ptr<ShaderProgram> m_shader;
        mutable std::vector<float> m_simplifyThreshold;
};


#endif // DISPLAZ_HCLOUDVIEW_H_INCLUDED
