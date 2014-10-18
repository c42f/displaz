#include "voxelizer.h"

#include <unistd.h>

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
void voxelizePointCloud(SimplePointDb& pointDb, float pointRadius,
                        const Imath::V3d& origin, double rootNodeWidth,
                        int leafDepth, int brickRes, Logger& logger)
{
    using Imath::V3i;
    // Useful references:
    //
    // Morton/z space filling curve ordering:
    // en.wikipedia.org/wiki/Z-order_curve
    //
    // "Coherent Out-of-Core Point-Based Global Illumination" by Kontkanen et al.,
    // Eurographics Symposium on Rendering 2011.

    // 1) Compute root node bounding box
    // 2) Decide on a chunk (2^N * 2^N group of bricks) size
    // 3) For every chunk in root bound,
    //    * Read points for chunk, (including pointRadius buffer area), binning
    //      their indices into bricks which are created on demand.
    //    * Render points for each brick
    //    * Sort bricks into Morton order
    //
    //    * Traverse ordering, producing higher level bricks as soon as they're
    //      complete, and dump them to per-level output queues.
    //
    //    * Each output queue flushes itself entirely when it reaches a given
    //      fill level.
    //
    //    * For every 
    //    * 
    //    * Iterate through a complete Morton ordering, picking up the next
    //      brick each time we get to its Morton index.
    //    * Each time we get a valid brick in a set of eight, compute the next
    //      higher level, connecting existing bricks as children in overall
    //      index
    Imath::Box3d rootBound(origin, origin + V3d(rootNodeWidth));

    double leafNodeWidth = rootNodeWidth/(1<<leafDepth);

    // Estimate chunk depth by assuming an essentially 2D point distribution 
    size_t desiredChunkPointCount = 1000000;
    double pointDensity = 50;  // points/m^2
    //int64_t expectedPoints = pointDb.estimatePointCount(rootBound);
    double expectedPoints = pointDensity*rootNodeWidth*rootNodeWidth;
    int chunkDepth = std::max(0, (int)ceil(log(expectedPoints/desiredChunkPointCount)/log(4)));

    logger.info("Depth of chunk root: %d", chunkDepth);
    logger.info("Estimated points per chunk: %d", expectedPoints/pow(4,chunkDepth));

    int chunkRes = 1<<chunkDepth;
    double chunkWidth = rootNodeWidth/chunkRes;
    int numChunks = chunkRes*chunkRes*chunkRes;

    logger.info("Tree depth in chunk: %d", leafDepth - chunkDepth);
    int chunkLeafRes = 1 << (leafDepth - chunkDepth);

    std::vector<float> position;
    std::vector<float> intensity;

    float invLeafNodeWidth = 1/leafNodeWidth;
    float fractionalPointRadius = pointRadius/leafNodeWidth;

    int leavesPerChunk = chunkLeafRes*chunkLeafRes*chunkLeafRes;

    logger.progress("Render chunks");
    OctreeBuilder builder(brickRes, leafDepth);
    // Traverse chunks in z order
    for (int chunkIdx = 0; chunkIdx < numChunks; ++chunkIdx)
    {
        logger.progress(double(chunkIdx)/(numChunks-1));
        V3i chunkPos = zOrderToVec3(chunkIdx);
        Imath::Box3d chunkBbox;
        chunkBbox.min = origin + chunkWidth*V3d(chunkPos);
        chunkBbox.max = origin + chunkWidth*V3d(chunkPos + V3i(1));
        Imath::Box3d bufferedBox = chunkBbox;
        bufferedBox.min -= V3d(pointRadius);
        bufferedBox.max += V3d(pointRadius);
        // Origin of chunk relative to overall cloud origin
        // FIXME: A fixed offset() doesn't make sense for really large clouds
        Imath::V3f relOrigin = chunkBbox.min - pointDb.offset();
        pointDb.query(bufferedBox, position, intensity);
        size_t numPoints = intensity.size();
        logger.debug("Chunk %d has %d points", chunkPos, numPoints);
        if (numPoints == 0)
            continue;

        // TODO: Early bailout when numPoints == 0.  Care must be taken here
        // in the octree build algorithm.

        // Bin point indices into full leaf node grid
        std::vector<std::vector<size_t>> leafIndices(leavesPerChunk);
        for (size_t pointIdx = 0; pointIdx < numPoints; ++pointIdx)
        {
            float x = invLeafNodeWidth*(position[3*pointIdx]   - relOrigin.x);
            float y = invLeafNodeWidth*(position[3*pointIdx+1] - relOrigin.y);
            float z = invLeafNodeWidth*(position[3*pointIdx+2] - relOrigin.z);
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
                leafIndices[idx].push_back(pointIdx);
            }
        }
        // Render points in each leaf into a MIP brick, in z curve order
        for (int i = 0; i < leavesPerChunk; ++i)
        {
            V3i leafPos = zOrderToVec3(i);
            const std::vector<size_t>& inds = leafIndices[
                (leafPos.z*chunkLeafRes + leafPos.y)*chunkLeafRes + leafPos.x];
            if (inds.empty())
                continue;
            float leafWidth = chunkWidth/chunkLeafRes;
            Imath::V3f leafMin = relOrigin + leafWidth*V3f(leafPos);
            std::unique_ptr<VoxelBrick> brick(new VoxelBrick(brickRes));
            brick->voxelizePoints(leafMin, leafWidth, pointRadius,
                                  position.data(), intensity.data(),
                                  inds.data(), inds.size());
            int64_t leafMortonIndex = chunkIdx*leavesPerChunk + i;
            builder.addNode(leafDepth, leafMortonIndex, std::move(brick));
            // Want to only store O(log(N)) full bricks - should be able to
            // achieve this, since we're traversing chunks in z order, and
            // leaves in z order.
            //
            // Need an octree data structure which persists over all chunks,
            // with the nodes storing bricks only for as long as they're
            // needed, but storing the data offsets from the output queues for
            // later dumping as an index.
        }
    }
    builder.finish();
}


