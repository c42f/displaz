// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DRAW_COST_MODEL_H_INCLUDED
#define DRAW_COST_MODEL_H_INCLUDED

#include <deque>
#include <vector>

#include "Geometry.h"
#include "util.h"

/// Frame time cost model for drawn geometry
///
/// We want to draw as much geometry per frame as possible, without the time
/// blowing out and the application becoming laggy.  This class models the
/// frame time as a function of an overall draw quality factor.  The same
/// quality factor is applied to all geometry to achive some consistency in the
/// amount of per-geometry quality degradation.
///
/// We model the cost of drawing geometry as the function
///
///   t(T,q) = a*Nv(T,q)
///
/// where
///   * t is the frame time
///   * T is the camera transformation
///   * q is the quality
///   * Nv is the number of vertices shaded
///
/// and a is an unknown fitting parameter which depends on the shader, speed of
/// the GPU etc.
class DrawCostModel
{
    public:
        DrawCostModel()
            : m_quality(1),
            m_incQuality(1),
            m_maxDrawRecords(20),
            m_drawRecords(),
            m_modelCoeffs(fitCostModel(m_drawRecords))
        { }

        double quality(double targetMillisecs,
                       const std::vector<const Geometry*>& geoms,
                       const TransformState& transState, bool firstIncrementalFrame);

        void addSample(const DrawCount& drawCount, double frameTime)
        {
            m_drawRecords.push_back(std::make_pair(drawCount, double(frameTime)));
            if ((int)m_drawRecords.size() > m_maxDrawRecords)
                m_drawRecords.pop_front();
            m_modelCoeffs = fitCostModel(m_drawRecords);
        }

    private:
        typedef std::deque<std::pair<DrawCount,double>> DrawRecords;

        static V3d fitCostModel(const DrawRecords& drawRecords);

        double m_quality;
        double m_incQuality;
        int m_maxDrawRecords;
        DrawRecords m_drawRecords;
        V3d m_modelCoeffs;
};


#endif // DRAW_COST_MODEL_H_INCLUDED
