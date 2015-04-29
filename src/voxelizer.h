// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_VOXELIZER_H_INCLUDED
#define DISPLAZ_VOXELIZER_H_INCLUDED

#include <algorithm>
#include <vector>

#include "hcloud.h"
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

        /// Serialize brick to output stream
        ///
        /// Return index node to the serialized data
        NodeIndexData serialize(std::ostream& out) const
        {
            // Serialize all voxels with nonzero coverage
            std::vector<float> positions;
            std::vector<float> coverage;
            std::vector<float> intensity;
            for (int i = 0, iend = numVoxels(); i < iend; ++i)
            {
                float cov = m_mipCoverage[i];
                if (cov != 0)
                {
                    positions.insert(positions.end(), &m_mipPosition[3*i],
                                     &m_mipPosition[3*i] + 3);
                    coverage.push_back(cov);
                    intensity.push_back(m_mipColor[i]);
                }
            }
            out.write((const char*)positions.data(), positions.size()*sizeof(float));
            out.write((const char*)coverage.data(),  coverage.size()*sizeof(float));
            out.write((const char*)intensity.data(), intensity.size()*sizeof(float));
            NodeIndexData indexData;
            indexData.numPoints = (uint32_t)coverage.size();
            indexData.flags = IndexFlags_Voxels;
            return indexData;
        }

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

/// Temporary container for leaf point data, for passing through to octree
/// builder
class LeafPointData
{
    public:
        LeafPointData(const float* position, const float* intensity,
                      const size_t* indices, size_t npoints)
            : m_position(position), m_intensity(intensity), m_indices(indices), m_npoints(npoints)
        { }

        NodeIndexData serialize(std::ostream& out) const
        {
            for (size_t i = 0; i < m_npoints; ++i)
                out.write((const char*)&m_position[3*m_indices[i]], 3*sizeof(float));
            for (size_t i = 0; i < m_npoints; ++i)
                out.write((const char*)&m_intensity[m_indices[i]], sizeof(float));
            NodeIndexData indexData;
            indexData.numPoints = (uint32_t)m_npoints;
            indexData.flags = IndexFlags_Points;
            return indexData;
        }

    private:
        const float* m_position;
        const float* m_intensity;
        const size_t* m_indices;
        size_t m_npoints;
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
        size_t pidx = pointIndices[pidxIdx];
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
