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

#ifndef DRAW_COST_MODEL_H_INCLUDED
#define DRAW_COST_MODEL_H_INCLUDED

#include <deque>
#include <vector>

#include "geometry.h"
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
