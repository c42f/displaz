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

#ifndef DISPLAZ_VOXELIZER_H_INCLUDED
#define DISPLAZ_VOXELIZER_H_INCLUDED

#include <algorithm>
#include <vector>

#include "util.h"

class SimplePointDb;
class Logger;


/// Convert ZCurve index to 3D vector of cell indices
inline Imath::V3i zOrderToVec3(int zIndex)
{
    Imath::V3i v(0);
    int i = 0;
    // Nonoptimal but simple method
    while (zIndex != 0)
    {
        v.x |= (zIndex & 1) << i;
        v.y |= ((zIndex>>1) & 1) << i;
        v.z |= ((zIndex>>2) & 1) << i;
        zIndex >>= 3;
        i += 1;
    }
    return v;
}


/// Create an octree by voxelizing points from `pointDb`
///
/// Voxelization occurs by rendering points from `pointDb` with radius
/// `pointRadius`.  The points are serialized, and written to `outputStream` in
/// hcloud format.
///
/// The bounding box of the octree will have a minimum at `origin` and a size
/// of `rootNodeWidth` in the three directions.  A fixed maximum octree depth
/// of `leafDepth` is used for the leaf nodes, each of which contains
/// brickRes*brickRes*brickRes voxels.
void voxelizePointCloud(std::ostream& outputStream,
                        SimplePointDb& pointDb, float pointRadius,
                        const Imath::V3d& origin, double rootNodeWidth,
                        int leafDepth, int brickRes, Logger& logger);


/// A 3D N*N*N array of voxels
class VoxelBrick
{
    public:
        VoxelBrick(int brickRes)
            : m_brickRes(brickRes),
            m_mipColor(m_brickRes*m_brickRes*m_brickRes, 0),
            m_mipCoverage(m_brickRes*m_brickRes*m_brickRes, 0),
            m_mipPosition(3*m_brickRes*m_brickRes*m_brickRes, 0)
        { }

        /// Return resolution of brick (ie, N, where brick has N*N*N voxels)
        int resolution() const { return m_brickRes; }

        /// Return total number of voxels in brick
        int numVoxels() const  { return m_brickRes*m_brickRes*m_brickRes; }

        float& coverage(int x, int y, int z)      { return m_mipCoverage[idx(x,y,z)]; }
        float coverage(int x, int y, int z) const { return m_mipCoverage[idx(x,y,z)]; }
        float coverage(int i) const               { return m_mipCoverage[i]; }

        V3f& position(int x, int y, int z) { return *reinterpret_cast<V3f*>(&m_mipPosition[3*idx(x,y,z)]); }
        const V3f& position(int x, int y, int z) const { return *reinterpret_cast<const V3f*>(&m_mipPosition[3*idx(x,y,z)]); }
        const V3f& position(int i) const { return *reinterpret_cast<const V3f*>(&m_mipPosition[3*i]); }

        float& color(int x, int y, int z)      { return m_mipColor[idx(x,y,z)]; }
        float color(int x, int y, int z) const { return m_mipColor[idx(x,y,z)]; }
        float color(int i) const               { return m_mipColor[i]; }

        /// Render given point set into the brick as voxels
        void voxelizePoints(const V3f& lowerCorner, float brickWidth,
                            float pointRadius,
                            const float* position, const float* intensity,
                            const size_t* pointIndices, int npoints);

        /// Render brick from a Morton ordered set of child bricks
        void renderFromBricks(VoxelBrick* children[8]);

    private:
        int m_brickRes;
        // Attributes for all voxels inside brick
        std::vector<float> m_mipColor;
        std::vector<float> m_mipCoverage;
        // Average position of points within brickmap voxels.  This greatly reduces
        // the octree terracing effect since it pulls points back to the correct
        // location even when the joins between octree levels cut through a
        // surface.  (Pixar discuss this effect in their application note on
        // brickmap usage.)
        std::vector<float> m_mipPosition;

        /// Return index of voxel at (x,y,z) in brick
        int idx(int x, int y, int z) const
        {
            assert(x >= 0 && x < m_brickRes && y >= 0 && y < m_brickRes && z >= 0 && z < m_brickRes);
            return x + m_brickRes*(y + m_brickRes*z);
        }
};


//------------------------------------------------------------------------------
/// Render points into raster, viewed orthographically from direction +z
///
/// intensityImage - intensity raster of size bufWidth*bufWidth
/// zbuf      - depth buffer of size bufWidth*bufWidth
/// bufWidth  - size of raster to render
/// xoff,yoff - origin of render buffer
/// pixelSize - Size of raster pixels in point coordinate system
/// px,py,pz  - Position x,y and z coordinates for each point
/// intensity - Intensity for each input point
/// radius    - Point radius in units of the point coordinate system
/// pointIndices - List of indices into px,py,pz,intensity, of length npoints
inline void orthoZRender(float* intensityImage, float* zbuf, int bufWidth,
                         float xoff, float yoff, float pixelSize,
                         const float* position, const float* intensity,
                         float radius, size_t* pointIndices, int npoints)
{
    memset(intensityImage, 0, sizeof(float)*bufWidth*bufWidth);
    for (int i = 0; i < bufWidth*bufWidth; ++i)
        zbuf[i] = -FLT_MAX;
    float invPixelSize = 1/pixelSize;
    float rPix = radius/pixelSize;
    for (int pidxIdx = 0; pidxIdx < npoints; ++pidxIdx)
    {
        int pidx = pointIndices[pidxIdx];
        float x = invPixelSize*(position[3*pidx] - xoff);
        float y = invPixelSize*(position[3*pidx+1] - yoff);
        float z = position[3*pidx+2];
        int x0 = (int)floor(x - rPix + 0.5);
        int y0 = (int)floor(y - rPix + 0.5);
        int x1 = (int)floor(x + rPix + 0.5);
        int y1 = (int)floor(y + rPix + 0.5);
        x0 = std::max(0, std::min(bufWidth, x0));
        y0 = std::max(0, std::min(bufWidth, y0));
        x1 = std::max(0, std::min(bufWidth, x1));
        y1 = std::max(0, std::min(bufWidth, y1));
        for (int yi = y0; yi < y1; ++yi)
        for (int xi = x0; xi < x1; ++xi)
        {
            int i = xi + yi*bufWidth;
            if (z > zbuf[i])
            {
                zbuf[i] = z;
                intensityImage[i] = intensity[pidx];
            }
        }
    }
}



#endif // DISPLAZ_VOXELIZER_H_INCLUDED
