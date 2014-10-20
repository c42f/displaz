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

#include <algorithm>
#include <fstream>
#include <queue>
#include <unordered_map>

#include "argparse.h"

#include "config.h"
#include "geomfield.h"
#include "hcloud.h"
#include "logger.h"
#include "util.h"
#include "pointdbwriter.h"
#include "pointdb.h"
#include "voxelizer.h"

//------------------------------------------------------------------------------
// File loading stuff.  TODO: Create a generic interface we can use instead.

#if 0
// Use laslib
#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable : 4996)
#   pragma warning(disable : 4267)
#elif __GNUC__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wstrict-aliasing"
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
// Note... laslib generates a small horde of warnings
#include <lasreader.hpp>
#ifdef _MSC_VER
#   pragma warning(push)
#elif __GNUC__
#   pragma GCC diagnostic pop
#if LAS_TOOLS_VERSION <= 120124
// Hack: kill gcc unused variable warning
class MonkeyChops { MonkeyChops() { (void)LAS_TOOLS_FORMAT_NAMES; } };
#endif
#endif


void loadLas(const std::string& fileName, size_t maxPointCount,
             std::vector<GeomField>& fields, V3d& offset,
             size_t& npoints, size_t& totPoints,
             Imath::Box3d& bbox, V3d& centroid, Logger& logger)
{
    V3d Psum(0);
    LASreadOpener lasReadOpener;
#ifdef _WIN32
    // Hack: liblas doesn't like forward slashes as path separators on windows
    fileName = fileName.replace('/', '\\');
#endif
    lasReadOpener.set_file_name(fileName.c_str());
    std::unique_ptr<LASreader> lasReader(lasReadOpener.open());

    if(!lasReader)
        throw std::runtime_error(tfm::format("Could not open file: %s", fileName));

    //std::ofstream dumpFile("points.txt");
    // Figure out how much to decimate the point cloud.
    totPoints = lasReader->header.number_of_point_records;
    size_t decimate = totPoints == 0 ? 1 : 1 + (totPoints - 1) / maxPointCount;
    npoints = (totPoints + decimate - 1) / decimate;
    logger.info("Loading %d of %d points from file %s",
                npoints, totPoints, fileName);
    offset = V3d(lasReader->header.min_x, lasReader->header.min_y, 0);
    // Attempt to place all data on the same vertical scale, but allow other
    // offsets if the magnitude of z is too large (and we would therefore loose
    // noticable precision by storing the data as floats)
    if (fabs(lasReader->header.min_z) > 10000)
        offset.z = lasReader->header.min_z;
    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    fields.push_back(GeomField(TypeSpec::uint16_i(), "intensity", npoints));
    // Iterate over all points & pull in the data.
    V3f* position           = (V3f*)fields[0].as<float>();
    uint16_t* intensity     = fields[1].as<uint16_t>();
    size_t readCount = 0;
    size_t storeCount = 0;
    size_t nextDecimateBlock = 1;
    size_t nextStore = 1;
    if (!lasReader->read_point())
    {
        throw std::runtime_error(
            tfm::format("Could not read first point from las file: %s", fileName));
    }
    const LASpoint& point = lasReader->point;
    uint16_t* color = 0;
    if (point.have_rgb)
    {
        fields.push_back(GeomField(TypeSpec(TypeSpec::Uint,2,3,TypeSpec::Color),
                                        "color", npoints));
        color = fields.back().as<uint16_t>();
    }
    logger.progress("Load file");
    do
    {
        // Read a point from the las file
        ++readCount;
        if(readCount % 10000 == 0)
            logger.progress(double(readCount)/totPoints);
        V3d P = V3d(point.get_x(), point.get_y(), point.get_z());
        bbox.extendBy(P);
        Psum += P;
        if(readCount < nextStore)
            continue;
        ++storeCount;
        // Store the point
        *position++ = P - offset;
        *intensity++ = point.intensity;
        // Extract point RGB
        if (color)
        {
            *color++ = point.rgb[0];
            *color++ = point.rgb[1];
            *color++ = point.rgb[2];
        }
        // Figure out which point will be the next stored point.
        nextDecimateBlock += decimate;
        nextStore = nextDecimateBlock;
        if(decimate > 1)
        {
            // Randomize selected point within block to avoid repeated patterns
            nextStore += (rand() % decimate);
            if(nextDecimateBlock <= totPoints && nextStore > totPoints)
                nextStore = totPoints;
        }
    }
    while(lasReader->read_point());
    lasReader->close();
    if (readCount < totPoints)
    {
        logger.warning("Expected %d points in file \"%s\", got %d",
                       totPoints, fileName, readCount);
        npoints = storeCount;
        // Shrink all fields to fit - these will have wasted space at the end,
        // but that will be fixed during reordering.
        for (size_t i = 0; i < fields.size(); ++i)
            fields[i].size = npoints;
        totPoints = readCount;
    }
    if (totPoints > 0)
        centroid = (1.0/totPoints)*Psum;
}


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
void orthoZRender(float* intensityImage, float* zbuf, int bufWidth,
                  float xoff, float yoff, float pixelSize,
                  const float* position, const uint16_t* intensity,
                  float radius, int* pointIndices, int npoints)
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


//------------------------------------------------------------------------------
/// Functor to compute octree child node index with respect to some given split
/// point
struct OctreeChildIdx
{
    const V3f* m_P;
    V3f m_center;
    int operator()(size_t i)
    {
        V3f p = m_P[i];
        return 4*(p.z >= m_center.z) +
               2*(p.y >= m_center.y) +
                 (p.x >= m_center.x);
    }

    OctreeChildIdx(const V3f* P, const V3f& center) : m_P(P), m_center(center) {}
};


const int brickN = 8;


struct MipBrick
{
    // Attributes for all voxels inside brick
    std::vector<float> m_mipColor;
    std::vector<float> m_mipCoverage;
    // Average position of points within brickmap voxels.  This greatly reduces
    // the octree terracing effect since it pulls points back to the correct
    // location even when the joins between octree levels cut through a
    // surface.  (Pixar discuss this effect in their application note on
    // brickmap usage.)
    std::vector<float> m_mipPosition;

    MipBrick()
        : m_mipColor(brickN*brickN*brickN, 0),
        m_mipCoverage(brickN*brickN*brickN, 0),
        m_mipPosition(3*brickN*brickN*brickN, 0)
    { }

    int idx(int x, int y, int z) const
    {
        assert(x >= 0 && x < brickN && y >= 0 && y < brickN && z >= 0 && z < brickN);
        return x + brickN*(y + brickN*z);
    }

    float& coverage(int x, int y, int z)
    {
        return m_mipCoverage[idx(x,y,z)];
    }
    float coverage(int x, int y, int z) const
    {
        return m_mipCoverage[idx(x,y,z)];
    }

    V3f& position(int x, int y, int z)
    {
        return *reinterpret_cast<V3f*>(&m_mipPosition[3*idx(x,y,z)]);
    }
    const V3f& position(int x, int y, int z) const
    {
        return *reinterpret_cast<const V3f*>(&m_mipPosition[3*idx(x,y,z)]);
    }

    float& color(int x, int y, int z)
    {
        return m_mipColor[idx(x,y,z)];
    }
    float color(int x, int y, int z) const
    {
        return m_mipColor[idx(x,y,z)];
    }

    void cacheFilledVoxels(float radius, std::vector<float>& positions,
                           std::vector<float>& coverages, std::vector<float>& colors)
    {
        size_t fillCount = std::count_if(
            m_mipCoverage.begin(), m_mipCoverage.end(),
            [](float c) { return c != 0; } );
        positions.reserve(3*fillCount);
        coverages.reserve(fillCount);
        colors.reserve(fillCount);
        for (int z = 0; z < brickN; ++z)
        for (int y = 0; y < brickN; ++y)
        for (int x = 0; x < brickN; ++x)
        {
            float c = coverage(x,y,z);
            if (c != 0)
            {
                positions.push_back(position(x,y,z).x);
                positions.push_back(position(x,y,z).y);
                positions.push_back(position(x,y,z).z);
                coverages.push_back(c);
                colors.push_back(color(x,y,z));
            }
        }
    }
};


struct OctreeNode
{
    OctreeNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    size_t beginIndex;       ///< Begin index of points in this node
    size_t endIndex;         ///< End index of points in this node
    V3f center;              ///< Centre of the node
    float radius;            ///< Distance to centre to node edge along an axis
    MipBrick brick;

    // List of non-empty voxels inside the brick
    std::vector<float> voxelPositions;
    std::vector<float> voxelColors;
    std::vector<float> voxelCoverage;

    // Variables used when dumping to disk
    mutable uint64_t dataOffset;

    OctreeNode(const V3f& center, float radius)
        : beginIndex(0), endIndex(0),
        center(center), radius(radius),
        dataOffset(0)
    {
        for (int i = 0; i < 8; ++i)
            children[i] = 0;
    }

    ~OctreeNode()
    {
        for (int i = 0; i < 8; ++i)
            delete children[i];
    }

    int numOccupiedVoxels() const { return voxelPositions.size()/3; }

    size_t numLeafPoints() const { return endIndex - beginIndex; }

    bool isLeaf() const { return beginIndex != endIndex; }

    void cacheFilledVoxels()
    {
        brick.cacheFilledVoxels(radius, voxelPositions, voxelCoverage, voxelColors);
    }
};


class ProgressCounter
{
    public:
        ProgressCounter(size_t pointCount) : m_totProcessed(0), m_pointCount(pointCount) {}

        void reset() { m_totProcessed = 0; }
        void add(size_t additionalProcessed) { m_totProcessed += additionalProcessed; }

        double progress() const { return double(m_totProcessed)/m_pointCount; }

    private:
        size_t m_totProcessed;
        size_t m_pointCount;
};


/// Create an octree over the given set of points with position P
///
/// The points for consideration in the current node are the set
/// P[inds[beginIndex..endIndex]]; the tree building process sorts the inds
/// array in place so that points for the output leaf nodes are held in
/// the range P[inds[node.beginIndex, node.endIndex)]].  center is the central
/// split point for splitting children of the current node; radius is the
/// current node radius measured along one of the axes.
///
/// TODO: Use bounding box here rather than centre and radius
static OctreeNode* makeTree(int depth, size_t* inds,
                            size_t beginIndex, size_t endIndex,
                            const V3f* P, const V3f& center,
                            float radius, double minNodeRadius,
                            Logger& logger, ProgressCounter& progCounter)
{
    OctreeNode* node = new OctreeNode(center, radius);
    // FIXME: Best cutoff size?
    const size_t pointsPerNode = 100;
    // Limit max depth of tree to prevent infinite recursion when
    // greater than pointsPerNode points lie at the same position in
    // space.  floats effectively have 24 bit of precision in the
    // mantissa, so there's never any point splitting more than 24 times.
    const int maxDepth = 24;
    size_t* beginPtr = inds + beginIndex;
    size_t* endPtr = inds + endIndex;
    if (endIndex - beginIndex <= pointsPerNode || depth >= maxDepth || radius < minNodeRadius)
    {
        std::random_shuffle(beginPtr, endPtr);
        node->beginIndex = beginIndex;
        node->endIndex = endIndex;
        progCounter.add(endIndex - beginIndex);
        logger.progress(progCounter.progress());
        return node;
    }
    // Partition points into the 8 child nodes
    size_t* childRanges[9];
    multi_partition(beginPtr, endPtr, OctreeChildIdx(P, center), childRanges+1, 8);
    childRanges[0] = beginPtr;
    // Recursively generate child nodes
    float r = radius/2;
    for (int i = 0; i < 8; ++i)
    {
        size_t childBeginIndex = childRanges[i]   - inds;
        size_t childEndIndex   = childRanges[i+1] - inds;
        if (childEndIndex == childBeginIndex)
            continue;
        V3f c = center + V3f((i     % 2 == 0) ? -r : r,
                             ((i/2) % 2 == 0) ? -r : r,
                             ((i/4) % 2 == 0) ? -r : r);
        node->children[i] = makeTree(depth+1, inds, childBeginIndex,
                                     childEndIndex, P, c, r, minNodeRadius,
                                     logger, progCounter);
    }
    return node;
}


/// Fill in octree with 3D mipmap "brick" data
static void fillBricks(OctreeNode* node, const V3f* P, const uint16_t* color,
                       float pointRadius, Logger& logger, ProgressCounter& progCounter)
{
    MipBrick& brick = node->brick;
    if (node->isLeaf())
    {
        V3f lowerCorner = node->center - V3f(node->radius);
        std::vector<std::vector<int>> layerInds(brickN);
        int npoints = node->endIndex - node->beginIndex;
        // Put points into brick voxel layers according to their position
        for (int i = 0; i < npoints; ++i)
        {
            V3f d = (1.0f*brickN/(2*node->radius))*(P[i+node->beginIndex] - lowerCorner);
            int layer = (int)d.z;
            // FIXME: Workaround for points occasionally being outside the node.
            if (layer < brickN)
                layerInds[layer].push_back(i);
        }
        const int pixPerVoxel = 4;
        const int rasterWidth = brickN*pixPerVoxel;
        const int npix = rasterWidth*rasterWidth;
        float raster[npix];
        float zbuf[npix];
        float* posRaw = (float*)(P+node->beginIndex);
        // For each layer, render raw points using orthographic projection from
        // +z direction at a higher resolution; average that to get voxel
        // values for voxels in the layer.
        //
        // TODO: Could use all eight cube normals for improved viewing from
        // other angles.  This is probably only useful for appreciable vertical
        // structure - need some nicely scanned cliffs or some such to test.
        //
        // TODO:
        // 1) Should put points redundently into sibiling nodes when those
        //    points are near the boundary.
        // 2) Frustum culling!
        float pixelSize = 2*node->radius/rasterWidth;
        for (int z = 0; z < brickN; ++z)
        {
            orthoZRender(raster, zbuf, rasterWidth,
                         lowerCorner.x, lowerCorner.y, pixelSize,
                         posRaw, color+node->beginIndex,
                         pointRadius, layerInds[z].data(), (int)layerInds[z].size());
            for (int y = 0; y < brickN; ++y)
            for (int x = 0; x < brickN; ++x)
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
                    brick.color(x,y,z) = colSum/sampCount;
                    brick.position(x,y,z) = V3f(xsum/sampCount + lowerCorner.x,
                                                ysum/sampCount + lowerCorner.y,
                                                zsum/sampCount);
                }
                brick.coverage(x,y,z) = (float)sampCount / (pixPerVoxel*pixPerVoxel);
            }
        }
        progCounter.add(npoints);
        logger.progress(progCounter.progress());
    }
    else
    {
        // Create internal node brick by downsampling child node bricks
        const int M = brickN/2;
        for (int childIdx = 0; childIdx < 8; ++childIdx)
        {
            OctreeNode* child = node->children[childIdx];
            if (!child)
                continue;
            fillBricks(child, P, color, pointRadius, logger, progCounter);
            const MipBrick& childBrick = child->brick;
            int xoff = M*(childIdx % 2);
            int yoff = M*((childIdx/2) % 2);
            int zoff = M*((childIdx/4) % 2);
            for (int z = 0; z < brickN; z+=2)
            for (int y = 0; y < brickN; y+=2)
            for (int x = 0; x < brickN; x+=2)
            {
                // 3D mipmap downsampling: Take eight adjacent samples and
                // accumulate them into a single new sample.  In contrast to 2D
                // mipmapping, opacity needs to be taken into account: one
                // layer can partially hide another.  In principle, this
                // introduces view dependence into the mipmap; here we assume
                // the viewer is roughly looking downward.
                float colSum = 0;
                V3f posSum = V3f(0);
                float covSum = 0;
                for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i)
                {
                    int x1 = x+i;
                    int y1 = y+j;
                    float c1 = childBrick.coverage(x1,y1,z+1);
                    float c0 = childBrick.coverage(x1,y1,z);
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
                    colSum += c0*childBrick.color(x1,y1,z)    + c1*childBrick.color(x1,y1,z+1);
                    posSum += c0*childBrick.position(x1,y1,z) + c1*childBrick.position(x1,y1,z+1);
                    covSum += c0 + c1;
                }
                if (covSum != 0)
                {
                    int x1 = x/2+xoff;
                    int y1 = y/2+yoff;
                    int z1 = z/2+zoff;
                    float w = 1.0f/covSum;
                    brick.color(x1, y1, z1)    = w*colSum;
                    brick.position(x1, y1, z1) = w*posSum;
                    // Note: Coverage is a special case: it's the average of
                    // coverage in the four child cells.
                    brick.coverage(x1, y1, z1) = covSum/4;
                }
            }
        }
    }
    node->cacheFilledVoxels();
}


/// Compute statistics about the octree rooted at rootNode
///
/// numNodes - Total number of nodes in the octree
/// numVoxels - Total number of voxels
static void treeStats(const OctreeNode* rootNode, uint64_t& numNodes, uint64_t& numVoxels)
{
    numNodes = 0;
    numVoxels = 0;
    std::vector<const OctreeNode*> nodeStack;
    nodeStack.push_back(rootNode);
    while (!nodeStack.empty())
    {
        const OctreeNode* node = nodeStack.back();
        nodeStack.pop_back();
        numNodes += 1;
        numVoxels += node->numOccupiedVoxels();
        for (int i = 0; i < 8; ++i)
        {
            OctreeNode* n = node->children[i];
            if (n)
                nodeStack.push_back(n);
        }
    }
}


//------------------------------------------------------------------------------


/// Search for named field with given number of channels in list of fields
int fieldSearch(const std::vector<GeomField>& fields,
                const std::string& name, int channelCount)
{
    for (size_t i = 0; i < fields.size(); ++i)
    {
        if (fields[i].name == name && fields[i].spec.count == channelCount)
            return i;
    }
    return -1;
}


/// Dump octree bricks to stream in displaz hierarchicial point cloud format
///
/// out        - destination stream
/// rootNode   - root of tree
/// boundingBox - bounding box of the data
/// treeBoundingBox - bounding box of root node
/// offset     - double precision origin offset for float point positions
/// leafPositions - leaf node position data
/// leafColors - leaf node color data
/// numPoints  - total number of points in leafPositions,leafColors
/// logger     - logging interface
void writeHcloud(std::ostream& out, const OctreeNode* rootNode,
                 const Imath::Box3d& boundingBox, const Imath::Box3d& treeBoundingBox,
                 const Imath::V3d& offset,
                 const V3f* leafPositions, const uint16_t* leafColors,
                 uint64_t numPoints, Logger& logger)
{
    // Number of bytes to hold a single point.
    // TODO: Compress!
    const int voxelPointSize = 3*sizeof(float) + sizeof(float) + sizeof(float);
    const int leafPointSize = 3*sizeof(float) + sizeof(float);
    // Number of bytes to hold an octree node in the index
    const int indexNodeSize = sizeof(uint8_t) + sizeof(uint64_t) + 2*sizeof(uint16_t);

    uint64_t numNodes, numVoxels;
    treeStats(rootNode, numNodes, numVoxels);

    HCloudHeader header;
    header.numPoints = numPoints;
    header.numVoxels = numVoxels;
    header.indexSize = indexNodeSize*numNodes;
    header.offset = offset;
    header.dataSize = voxelPointSize*numVoxels + leafPointSize*numPoints;
    header.boundingBox = boundingBox;
    header.treeBoundingBox = treeBoundingBox;
    header.brickSize = brickN;
    header.write(out);

    logger.info("Header:\n%s", header);

    // Dump all octree nodes at the same level into contiguous locations using
    // a breadth-first traversal so that associated data can be read together.
    logger.progress("Write data");
    out.seekp(header.headerSize + header.indexSize);
    uint64_t dataOffset = 0;
    std::queue<const OctreeNode*> nodeQueue;
    nodeQueue.push(rootNode);
    while (!nodeQueue.empty())
    {
        const OctreeNode* node = nodeQueue.front();
        nodeQueue.pop();
        node->dataOffset = dataOffset;
        dataOffset += voxelPointSize * node->numOccupiedVoxels() +
                      leafPointSize  * (node->endIndex - node->beginIndex);
        out.write((const char*)node->voxelPositions.data(),
                  node->voxelPositions.size()*sizeof(float));
        out.write((const char*)node->voxelCoverage.data(),
                  node->voxelCoverage.size()*sizeof(float));
        out.write((const char*)node->voxelColors.data(),
                  node->voxelColors.size()*sizeof(float));
        if (node->isLeaf())
        {
            out.write((const char*)(leafPositions + node->beginIndex),
                      node->numLeafPoints()*3*sizeof(float));
            out.write((const char*)(leafColors + node->beginIndex),
                      node->numLeafPoints()*sizeof(float));
        }
        assert((uint64_t)out.tellp() == dataOffset + header.headerSize + header.indexSize);
        logger.progress(double(dataOffset)/header.dataSize);
        for (int i = 0; i < 8; ++i)
        {
            const OctreeNode* n = node->children[i];
            if (n)
                nodeQueue.push(n);
        }
    }

    // Dump octree structure in depth-first order for simplicity.
    logger.progress("Write index");
    out.seekp(header.headerSize);
    std::vector<const OctreeNode*> nodeStack;
    nodeStack.push_back(rootNode);
    size_t indexNodesDumped = 0;
    while (!nodeStack.empty())
    {
        const OctreeNode* node = nodeStack.back();
        nodeStack.pop_back();
        uint8_t childNodeMask = 0;
        // Pack presence of children as bits into a uint8_t
        for (int i = 0; i < 8; ++i)
            childNodeMask |= (node->children[i] != 0) << i;
        writeLE<uint8_t> (out, childNodeMask);
        writeLE<uint64_t>(out, node->dataOffset);
        writeLE<uint16_t>(out, node->numOccupiedVoxels());
        writeLE<uint16_t>(out, node->numLeafPoints());
        indexNodesDumped += 1;
        logger.progress(double(indexNodesDumped)/numNodes);
        // Note backward iteration here is to ensure children are ordered from
        // 0 to 7 on disk (would be less confusing with a recursive function).
        for (int i = 7; i >= 0; --i)
        {
            const OctreeNode* n = node->children[i];
            if (n)
                nodeStack.push_back(n);
        }
    }
}


/// Load point cloud and save a voxelized LoD representation to output file
void voxelizePointCloud(const std::string& inFileName, const std::string& outFileName,
                        double pointRadius, double minNodeRadius, Logger& logger)
{
    size_t maxPointCount = 1000000000;
    size_t totPoints = 0;
    Imath::Box3d bbox;
    V3d offset = V3d(0);
    V3d centroid = V3d(0);
    size_t npoints = 0;
    std::vector<GeomField> fields;
    loadLas(inFileName, maxPointCount, fields, offset,
            npoints, totPoints, bbox, centroid, logger);
    // Search for position field
    int positionFieldIdx = fieldSearch(fields, "position", 3);
    if (positionFieldIdx == -1)
        throw std::runtime_error(tfm::format("No position field found in file %s", inFileName));
    const V3f* position = (V3f*)fields[positionFieldIdx].as<float>();
    if (totPoints == 0)
        throw std::runtime_error("No points found");

    // Sort points into octree order
    ProgressCounter progCounter(npoints);
    logger.progress("Sorting points");
    std::unique_ptr<size_t[]> inds(new size_t[npoints]);
    for (size_t i = 0; i < npoints; ++i)
        inds[i] = i;
    // Expand the bound so that it's cubic.  Cubic nodes work better because
    // the points are better distributed for LoD, splitting is unbiased, etc.
    Imath::Box3d rootBound(bbox.min, bbox.max);
    const V3d diag = rootBound.size();
    double rootRadius = std::max(std::max(diag.x, diag.y), diag.z) / 2;
    const V3d center = rootBound.center();
    rootBound.min = center - V3d(rootRadius);
    rootBound.max = center + V3d(rootRadius);
    std::unique_ptr<OctreeNode> rootNode;
    rootNode.reset(makeTree(0, &inds[0], 0, npoints, &position[0],
                            center - offset, rootRadius, minNodeRadius,
                            logger, progCounter));
    // Reorder point fields into octree order
    logger.progress("Reordering fields");
    for (size_t i = 0; i < fields.size(); ++i)
    {
        reorder(fields[i], inds.get(), npoints);
        logger.progress(double(i+1)/fields.size());
    }
    position = (V3f*)fields[positionFieldIdx].as<float>();

    int colorFieldIdx = fieldSearch(fields, "intensity", 1);
    if (colorFieldIdx == -1)
        throw std::runtime_error("Could not find intensity field in file");
    const uint16_t* color = fields[colorFieldIdx].as<uint16_t>();
    progCounter.reset();
    logger.progress("Computing bricks");
    fillBricks(rootNode.get(), position, color, pointRadius, logger, progCounter);

    std::ofstream outFile(outFileName.c_str(), std::ios::binary);
    if (!outFile)
        throw std::runtime_error("Could not open output file");
    writeHcloud(outFile, rootNode.get(), bbox, rootBound, offset, position, color, npoints, logger);
}
#endif


std::vector<std::string> g_positionalArgs;
static int storePositionalArg (int argc, const char *argv[])
{
    for(int i = 0; i < argc; ++i)
        g_positionalArgs.push_back (argv[i]);
    return 0;
}


/// dvox: A batch voxelizer for unstructured point clouds
int main(int argc, char* argv[])
{
    Imath::V3d boundMin = Imath::V3d(0);
    double rootNodeWidth = 1000;
    float pointRadius = 0.2;
    int brickRes = 8;
    double leafNodeWidth = 2.5;

    double dbTileSize = 100;
    double dbCacheSize = 100;

    bool logProgress = false;
    int logLevel = Logger::Info;

    ArgParse::ArgParse ap;

    ap.options(
        "dvox - voxelize unstructured point clouds (version " DISPLAZ_VERSION_STRING ")\n"
        "\n"
        "Usage: dvox input1 [input2 ...] output\n"
        "\n"
        "input can be .las or .pointdb\n"
        "output can be .pointdb or .hcloud",
        "%*", storePositionalArg, "",

        "<SEPARATOR>", "\nVoxelization Options:",
        "-bound %F %F %F %F", &boundMin.x, &boundMin.y, &boundMin.z, &rootNodeWidth,
                                        "Bounding box for hcloud (min_x min_y min_z width)",
        "-pointradius %f", &pointRadius, "Assumed radius of points used during voxelization",
        "-brickresolution %d", &brickRes, "Resolution of octree bricks",
        "-leafnoderadius %F", &leafNodeWidth, "Desired width for octree leaf nodes",

        "<SEPARATOR>", "\nPoint Database options:",
        "-dbtilesize %F", &dbTileSize, "Tile size of temporary point database",
        "-dbcachesize %F", &dbCacheSize, "In-memory cache size for database in MB (default 100 MB)",

        "<SEPARATOR>", "\nInformational options:",
        "-loglevel %d",  &logLevel,    "Logger verbosity (default 3 = info, greater is more verbose)",
        "-progress",     &logProgress, "Log processing progress",

        NULL
    );

    StreamLogger logger(std::cerr);

    if (ap.parse(argc, const_cast<const char**>(argv)) < 0)
    {
        ap.usage();
        logger.error("%s", ap.geterror());
        return EXIT_FAILURE;
    }
    if (argc == 1)
    {
        ap.usage();
        return EXIT_FAILURE;
    }

    logger.setLogLevel(Logger::LogLevel(logLevel));
    logger.setLogProgress(logProgress);

    try
    {
        if (g_positionalArgs.size() < 2)
        {
            logger.error("Expected at least two positional arguments");
            ap.usage();
            return EXIT_FAILURE;
        }
        std::string outputPath = g_positionalArgs.back();
        std::vector<std::string> inputPaths(g_positionalArgs.begin(),
                                            g_positionalArgs.end()-1);
        if (endswith(outputPath, ".pointdb"))
        {
            convertLasToPointDb(outputPath, inputPaths,
                                Imath::Box3d(), dbTileSize, logger);
        }
        else
        {
            if (!endswith(g_positionalArgs[0], ".pointdb") ||
                inputPaths.size() != 1)
            {
                logger.error("Need exactly one input .pointdb file");
                return EXIT_FAILURE;
            }
            if (!endswith(outputPath, ".hcloud"))
            {
                logger.error("Expected .hcloud file as output path");
                return EXIT_FAILURE;
            }
            SimplePointDb pointDb(inputPaths[0],
                                  (size_t)(dbCacheSize*1024*1024),
                                  logger);

            int leafDepth = floor(log(rootNodeWidth/leafNodeWidth)/log(2) + 0.5);
            logger.info("Leaf node width = %.3f", rootNodeWidth / (1 << leafDepth));
            std::ofstream outputFile(outputPath);
            voxelizePointCloud(outputFile, pointDb, pointRadius,
                               boundMin, rootNodeWidth,
                               leafDepth, brickRes, logger);
        }
    }
    catch (std::exception& e)
    {
        logger.error("Caught exception: %s", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

