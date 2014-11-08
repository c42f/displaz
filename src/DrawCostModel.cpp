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

#include "DrawCostModel.h"

// Figure out quality we should use to render points with the current camera
// transformation
double DrawCostModel::quality(double targetMillisecs,
                              const std::vector<const Geometry*>& geoms,
                              const TransformState& transState, bool firstIncrementalFrame)
{
    const int numQualitySamps = 4;
    double quality = firstIncrementalFrame ? m_incQuality : m_quality;
    double qualities[numQualitySamps] = {quality/20, quality/4, quality, quality*4};
    DrawCount drawCounts[numQualitySamps];
    for(size_t i = 0; i < geoms.size(); ++i)
    {
        geoms[i]->estimateCost(transState, firstIncrementalFrame,
                               qualities, drawCounts, numQualitySamps);
    }

    double tEst[numQualitySamps] = {0};
    for (int i = 0; i < numQualitySamps; ++i)
    {
        V3d v(1, drawCounts[i].numVertices, drawCounts[i].numFragments);
        tEst[i] = m_modelCoeffs.dot(v);
    }
    bool expectMoreToDraw = false;
    if (targetMillisecs <= tEst[0])
    {
        quality = qualities[0];
        expectMoreToDraw = drawCounts[0].moreToDraw;
    }
    else if (targetMillisecs >= tEst[numQualitySamps-1])
    {
        quality = qualities[numQualitySamps-1];
        expectMoreToDraw = drawCounts[numQualitySamps-1].moreToDraw;
    }
    else
    {
        // Find desired quality by basic linear interpolation
        int i = 0;
        while (i < numQualitySamps-2 && tEst[i+1] < targetMillisecs)
            ++i;
        double interp = (targetMillisecs - tEst[i]) / (tEst[i+1] - tEst[i]);
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
//    // Initialize sums to nonzero for a weak conservative regularization:
//    // assert that we can draw nvsum vertices in tsum millisecs.
//    double nvsum = 1000000;
//    double tsum = 50;
//
//    double regWeight = 1e-3;
//    nvsum *= regWeight;
//    tsum *= regWeight;
//
//    int numDrawRecs = (int)drawRecords.size();
//    for (int i = 0; i < numDrawRecs; ++i)
//    {
//        // Weight in favour of recent measurements.  Over 100 measurements,
//        // this reduces the last measurement to a weight of ~0.1
//        double w = exp(-0.2*(numDrawRecs - 1 - i));
//        auto& rec = drawRecords[i];
//        nvsum += w*rec.first.numVertices;
//        tsum += w*rec.second;
//    }
//
//    return V3d(0, tsum/nvsum, 0);


    Imath::M33d ATA(0.0);
    Imath::V3d ATt(0.0);

    int numDrawRecs = (int)drawRecords.size();
    for (int i = 0; i < numDrawRecs; ++i)
    {
        // Weight in favour of recent measurements.  Over 100 measurements,
        // this reduces the last measurement to a weight of ~0.1
        double w = exp(-0.023*(numDrawRecs - 1 - i));
        auto& rec = drawRecords[i];
        double v0 = 1;
        double v1 = rec.first.numVertices;
        double v2 = rec.first.numFragments;
        double t = rec.second;
        ATA[0][0] += w*v0*v0;
        ATA[0][1] += w*v0*v1;
        ATA[0][2] += w*v0*v2;
        ATA[1][1] += w*v1*v1;
        ATA[1][2] += w*v1*v2;
        ATA[2][2] += w*v2*v2;

        ATt[0] += w*v0*t;
        ATt[1] += w*v1*t;
        ATt[2] += w*v2*t;
    }
    // Fill in lower triangular part of symmetric ATA
    ATA[1][0] = ATA[0][1];
    ATA[2][0] = ATA[0][2];
    ATA[2][1] = ATA[1][2];

    // Ick!  Imath doesn't have a solve() function, so invert A :-(
    V3d modelCoeffs = ATt*ATA.invert();

    // Robustness hack: clamp costs to be positive.  (Should possibly do
    // nonegative least squares, but the above is already complex enough
    // without a good matrix library.)
    modelCoeffs.x = std::max(0.0, modelCoeffs.x);
    modelCoeffs.y = std::max(0.0, modelCoeffs.y);
    modelCoeffs.z = std::max(0.0, modelCoeffs.z);

    return modelCoeffs;
}


