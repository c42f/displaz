// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "voxelizer.h"

#include "hcloud.h"
#include "logger.h"
#include "octreebuilder.h"
#include "pointdb.h"

void VoxelBrick::voxelizePoints(const V3f& lowerCorner, float brickWidth,
                                float pointRadius,
                                const float* position, const float* intensity,
                                const size_t* pointIndices, int npoints)
{
    float invVoxelWidth = m_brickRes/brickWidth;
    // Sort points into brick voxel layers according to their position
    std::vector<std::vector<size_t>> layerInds(m_brickRes);
    for (int i = 0; i < npoints; ++i)
    {
        float pz = position[3*pointIndices[i] + 2];
        int layer = Imath::clamp((int)floor(invVoxelWidth*(pz - lowerCorner.z)),
                                 0, m_brickRes-1);
        layerInds[layer].push_back(pointIndices[i]);
    }
    const int pixPerVoxel = 4;  // TODO: Make settable
    const int rasterWidth = m_brickRes*pixPerVoxel;
    const int npix = rasterWidth*rasterWidth;
    std::vector<float> raster(npix);
    std::vector<float> zbuf(npix);
    // For each layer, render raw points using orthographic projection from
    // +z direction at a higher resolution; average that to get voxel
    // values for voxels in the layer.
    //
    // TODO: Could use all eight cube normals for improved viewing from
    // other angles.  This is probably only useful for appreciable vertical
    // structure - need some nicely scanned cliffs or some such to test.
    float pixelSize = brickWidth/rasterWidth;
    int coveredCount = 0;
    for (int z = 0; z < m_brickRes; ++z)
    {
        orthoZRender(raster.data(), zbuf.data(), rasterWidth,
                     lowerCorner.x, lowerCorner.y, pixelSize,
                     position, intensity, pointRadius,
                     layerInds[z].data(), (int)layerInds[z].size());
        for (int y = 0; y < m_brickRes; ++y)
        for (int x = 0; x < m_brickRes; ++x)
        {
            // Average rendered color over the voxel surface, and insert
            // into brick along with coverage
            int sampCount = 0;
            float colSum = 0;
            float zsum = 0;
            float xsum = 0;
            float ysum = 0;
            for (int j = 0; j < pixPerVoxel; ++j)
            for (int i = 0; i < pixPerVoxel; ++i)
            {
                int idx = x*pixPerVoxel + i + (y*pixPerVoxel + j)*rasterWidth;
                if (zbuf[idx] != -FLT_MAX)
                {
                    colSum += raster[idx];
                    zsum += zbuf[idx];
                    xsum += pixelSize*(x*pixPerVoxel + i + 0.5f);
                    ysum += pixelSize*(y*pixPerVoxel + j + 0.5f);
                    sampCount += 1;
                }
            }
            if (sampCount != 0)
            {
                this->color(x,y,z) = colSum/sampCount;
                this->position(x,y,z) = V3f(xsum/sampCount + lowerCorner.x,
                                            ysum/sampCount + lowerCorner.y,
                                            zsum/sampCount);
                ++coveredCount;
            }
            this->coverage(x,y,z) = (float)sampCount / (pixPerVoxel*pixPerVoxel);
        }
    }
}


void VoxelBrick::renderFromBricks(VoxelBrick* children[8])
{
    // Create internal node brick by downsampling child node bricks
    const int M = m_brickRes/2;
    for (int childIdx = 0; childIdx < 8; ++childIdx)
    {
        VoxelBrick* child = children[childIdx];
        if (!child)
            continue;
        assert(child->m_brickRes == m_brickRes);
        Imath::V3i childPos = zOrderToVec3(childIdx);
        int xoff = M*childPos.x;
        int yoff = M*childPos.y;
        int zoff = M*childPos.z;
        for (int z = 0; z < m_brickRes; z+=2)
        for (int y = 0; y < m_brickRes; y+=2)
        for (int x = 0; x < m_brickRes; x+=2)
        {
            // 3D mipmap downsampling: Take eight adjacent samples and
            // accumulate them into a single new sample.  In contrast to 2D
            // mipmapping, opacity needs to be taken into account: one
            // layer can partially hide another.  In principle, this
            // introduces view dependence into the mipmap; here we assume
            // the viewer is roughly looking downward.
            float colSum = 0;
            V3f posSum = V3f(0);
            float coverageSum = 0;
            for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i)
            {
                int x1 = x+i;
                int y1 = y+j;
                float c1 = child->coverage(x1,y1,z+1);
                float c0 = child->coverage(x1,y1,z);
                // Unusual compositing rule: assume geometry is perfectly
                // *coherent* and complementary, so opacities add.  This is
                // the correct rule for linear surfaces which pass through
                // the child bricks.  In the general case, it overestimates
                // opacity but that's probably less bad than having
                // semi-transparent surfaces all over the place.
                //
                // TODO: This doesn't seem to have a huge impact in
                // practise; decide whether it's a good idea, and whether
                // there's a lenght scale where we should switch between
                // the rules.
                c0 = std::min(1-c1, c0);
                //c0 = (1-c1)*c0;  // Usual compositing rule for incoherent geometry
                colSum += c0*child->color(x1,y1,z)    + c1*child->color(x1,y1,z+1);
                posSum += c0*child->position(x1,y1,z) + c1*child->position(x1,y1,z+1);
                coverageSum += c0 + c1;
            }
            if (coverageSum != 0)
            {
                int x1 = x/2+xoff;
                int y1 = y/2+yoff;
                int z1 = z/2+zoff;
                float w = 1.0f/coverageSum;
                this->color(x1, y1, z1)    = w*colSum;
                this->position(x1, y1, z1) = w*posSum;
                // Note: Coverage is a special case: it's the average of
                // coverage in the four child cells.
                this->coverage(x1, y1, z1) = coverageSum/4;
            }
        }
    }
}


//------------------------------------------------------------------------------
void voxelizePointCloud(std::ostream& outputStream,
                        SimplePointDb& pointDb, float pointRadius,
                        const Imath::V3d& origin, double rootNodeWidth,
                        int leafDepth, int brickRes, Logger& logger)
{
    // Bottom up octree build algorithm.  Each octree node contains a "brick"
    // of M*M*M voxels which are a level-of-detail representation of all points
    // inside the node.  The root bounding box is filled with a cubic grid of
    // 2^N * 2^N * 2^N leaf bricks where N is the depth of the octree.
    //
    // Building the tree proceeds as follows:
    //
    // * Leaf nodes are traversed in Morton order.
    //
    // * For each leaf node, the points inside the bounding box are extracted
    //   from the point database and rendered into the brick voxels.  (In
    //   practise point extraction happens on a chunk level to avoid a lot of
    //   very small database queries.)
    //
    // * Whenever a group of 8 adjacent nodes are complete, these are
    //   downsampled to produce the next coarser level of detail in the tree.
    //   They are then serialized to an output buffer and deallocated. Index
    //   information for the location of the nodes in the binary stream is
    //   saved during serialization.
    //
    // * When all nodes have been extracted, the index is also serialized.
    //
    //
    // Useful references:
    //
    // Morton/z space filling curve ordering:
    // en.wikipedia.org/wiki/Z-order_curve
    //
    // "Coherent Out-of-Core Point-Based Global Illumination" by Kontkanen et al.,
    // Eurographics Symposium on Rendering 2011.

    Imath::Box3d rootBound(origin, origin + V3d(rootNodeWidth));

    double leafNodeWidth = rootNodeWidth/(1<<leafDepth);

    // Estimate chunk depth by assuming an essentially 2D point distribution 
    size_t desiredChunkPointCount = 1000000;
    double pointDensity = 50;  // points/m^2
    //int64_t expectedPoints = pointDb.estimatePointCount(rootBound);
    double expectedPoints = pointDensity*rootNodeWidth*rootNodeWidth;
    int chunkDepth = std::max(0, (int)ceil(log(expectedPoints/desiredChunkPointCount)/log(4)));

    logger.info("Tree leaf depth: %d", leafDepth);
    logger.info("Depth of chunk root: %d", chunkDepth);
    logger.info("Estimated points per chunk: %d", expectedPoints/pow(4,chunkDepth));

    int chunkRes = 1<<chunkDepth;
    double chunkWidth = rootNodeWidth/chunkRes;
    int numChunks = chunkRes*chunkRes*chunkRes;

    int chunkLeafRes = 1 << (leafDepth - chunkDepth);

    std::vector<float> position;
    std::vector<float> intensity;

    double invLeafNodeWidth = 1/leafNodeWidth;
    double fractionalPointRadius = pointRadius/leafNodeWidth;

    int leavesPerChunk = chunkLeafRes*chunkLeafRes*chunkLeafRes;

    logger.progress("Render chunks");
    OctreeBuilder builder(outputStream, brickRes, leafDepth, pointDb.offset(),
                          rootBound, logger);
    // Traverse chunks in z order
    for (int chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx)
    {
        logger.progress(double(chunkIdx)/(numChunks-1));
        Imath::V3i chunkPos = zOrderToVec3(chunkIdx);
        Imath::Box3d chunkBbox;
        chunkBbox.min = origin + chunkWidth*V3d(chunkPos);
        chunkBbox.max = origin + chunkWidth*V3d(chunkPos + Imath::V3i(1));
        Imath::Box3d bufferedBox = chunkBbox;
        bufferedBox.min -= V3d(pointRadius);
        bufferedBox.max += V3d(pointRadius);
        // Origin of chunk relative to overall cloud origin
        // FIXME: A fixed offset() doesn't make sense for really large clouds
        Imath::V3d relOrigin = chunkBbox.min - pointDb.offset();
        pointDb.query(bufferedBox, position, intensity);
        size_t numPoints = intensity.size();
        logger.debug("Chunk %d has %d points", chunkPos, numPoints);
        if (numPoints == 0)
            continue;

        // TODO: Early bailout when numPoints == 0.  Care must be taken here
        // in the octree build algorithm.

        // Bin point indices into full leaf node grid.
        // leafIndices[i] has the indices for points in the ith leaf, where i
        // is a lexicographic ordering (since that's simpler to compute than the
        // Morton order)
        std::vector<std::vector<size_t>> bufferedLeafIndices(leavesPerChunk);
        std::vector<std::vector<size_t>> leafIndices(leavesPerChunk);
        for (size_t pointIdx = 0; pointIdx < numPoints; ++pointIdx)
        {
            // Record point in all leaf nodes it touches out to the point radius
            double x = invLeafNodeWidth*(position[3*pointIdx]   - relOrigin.x);
            double y = invLeafNodeWidth*(position[3*pointIdx+1] - relOrigin.y);
            double z = invLeafNodeWidth*(position[3*pointIdx+2] - relOrigin.z);
            int xbegin = Imath::clamp((int)floor(x - fractionalPointRadius), 0, chunkLeafRes);
            int xend   = Imath::clamp((int)ceil (x + fractionalPointRadius), 0, chunkLeafRes);
            int ybegin = Imath::clamp((int)floor(y - fractionalPointRadius), 0, chunkLeafRes);
            int yend   = Imath::clamp((int)ceil (y + fractionalPointRadius), 0, chunkLeafRes);
            int zbegin = Imath::clamp((int)floor(z - fractionalPointRadius), 0, chunkLeafRes);
            int zend   = Imath::clamp((int)ceil (z + fractionalPointRadius), 0, chunkLeafRes);
            for (int zi = zbegin; zi < zend; ++zi)
            for (int yi = ybegin; yi < yend; ++yi)
            for (int xi = xbegin; xi < xend; ++xi)
            {
                int idx = (zi*chunkLeafRes + yi)*chunkLeafRes + xi;
                bufferedLeafIndices[idx].push_back(pointIdx);
            }
            // Record point in leaf node in which it actually resides
            int xi = (int)floor(x);
            int yi = (int)floor(y);
            int zi = (int)floor(z);
            if (xi >= 0 && xi < chunkLeafRes &&
                yi >= 0 && yi < chunkLeafRes &&
                zi >= 0 && zi < chunkLeafRes)
            {
                int idx = (zi*chunkLeafRes + yi)*chunkLeafRes + xi;
                leafIndices[idx].push_back(pointIdx);
            }
        }
        // Render points in each leaf into a MIP brick in z curve order, and
        // dump to output.  Since we're traversing both chunks and leaves in
        // Morton order, the leaves are traversed in Morton order as a whole.
        for (int leafIdx = 0; leafIdx < leavesPerChunk; ++leafIdx)
        {
            Imath::V3i leafPos = zOrderToVec3(leafIdx);
            int lexLeafIdx = (leafPos.z*chunkLeafRes + leafPos.y)*chunkLeafRes + leafPos.x;
            const std::vector<size_t>& bufferedInds = bufferedLeafIndices[lexLeafIdx];
            const std::vector<size_t>& inds = leafIndices[lexLeafIdx];
            if (bufferedInds.empty())
                continue;
            double leafWidth = chunkWidth/chunkLeafRes;
            Imath::V3f leafMin = relOrigin + leafWidth*V3d(leafPos);
            std::unique_ptr<VoxelBrick> brick(new VoxelBrick(brickRes));
            brick->voxelizePoints(leafMin, (float)leafWidth, pointRadius,
                                  position.data(), intensity.data(),
                                  bufferedInds.data(), (int)bufferedInds.size());
            LeafPointData leafPointData(position.data(), intensity.data(),
                                        inds.data(), inds.size());
            int64_t leafMortonIndex = chunkIdx*leavesPerChunk + leafIdx;
            builder.addNode(leafDepth, leafMortonIndex, std::move(brick),
                            leafPointData);
        }
    }
    builder.finish();
}


