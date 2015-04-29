// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "DrawCostModel.h"

// Figure out quality we should use to render points with the current camera
// transformation
double DrawCostModel::quality(double targetMillisecs,
                              const std::vector<const Geometry*>& geoms,
                              const TransformState& transState,
                              bool firstIncrementalFrame)
{
    // Sample draw count function at various qualities
    const int numQualitySamps = 4;
    double quality = firstIncrementalFrame ? m_incQuality : m_quality;
    double qualities[numQualitySamps] = {quality/20, quality/4, quality, quality*4};
    DrawCount drawCounts[numQualitySamps];
    for(size_t i = 0; i < geoms.size(); ++i)
    {
        geoms[i]->estimateCost(transState, firstIncrementalFrame,
                               qualities, drawCounts, numQualitySamps);
    }
    // Estimate frame time at each quality
    double frameTimeEst[numQualitySamps] = {0};
    for (int i = 0; i < numQualitySamps; ++i)
    {
        V3d v(1, drawCounts[i].numVertices, 0);
        frameTimeEst[i] = m_modelCoeffs.dot(v);
    }

    // Interpolate desired quality using guess at the frame time
    bool expectMoreToDraw = false;
    if (targetMillisecs <= frameTimeEst[0])
    {
        quality = qualities[0];
        expectMoreToDraw = drawCounts[0].moreToDraw;
    }
    else if (targetMillisecs >= frameTimeEst[numQualitySamps-1])
    {
        quality = qualities[numQualitySamps-1];
        expectMoreToDraw = drawCounts[numQualitySamps-1].moreToDraw;
    }
    else
    {
        // Find desired quality by basic linear interpolation
        int i = 0;
        while (i < numQualitySamps-2 && frameTimeEst[i+1] < targetMillisecs)
            ++i;
        double interp = (targetMillisecs - frameTimeEst[i]) /
                        (frameTimeEst[i+1] - frameTimeEst[i]);
        quality = (1-interp)*qualities[i] + interp*qualities[i+1];
        expectMoreToDraw = drawCounts[i].moreToDraw;
    }
    if (expectMoreToDraw && !firstIncrementalFrame)
    {
        // Only update reference quality when increasing the quality would
        // result in more points being drawn to avoid the quality increasing
        // without bound
        m_quality = quality;
    }
    m_incQuality = quality;
    return quality;
}


V3d DrawCostModel::fitCostModel(const DrawRecords& drawRecords)
{
    // Initialize sums to nonzero for a weak conservative regularization:
    // assert that we can draw nvsum vertices in tsum millisecs.
    double nvsum = 1000000;
    double tsum = 50;

    double regWeight = 1e-3;
    nvsum *= regWeight;
    tsum *= regWeight;

    int numDrawRecs = (int)drawRecords.size();
    for (int i = 0; i < numDrawRecs; ++i)
    {
        // Weight in favour of recent measurements.
        double w = exp(-0.2*(numDrawRecs - 1 - i));
        auto& rec = drawRecords[i];
        nvsum += w*rec.first.numVertices;
        tsum += w*rec.second;
    }

    return V3d(0, tsum/nvsum, 0);
}


