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

#include "pointarray.h"

#include "glutil.h"

#include <QtGui/QMessageBox>
#include <QtOpenGL/QGLShaderProgram>
#include <QtCore/QTime>

#include <unordered_map>
#include <fstream>

#include "tinyformat.h"


#ifdef DISPLAZ_USE_PDAL

// OpenEXR unfortunately #defines restrict, which clashes with PDAL's use of
// boost.iostreams which uses "restrict" as an identifier.
#ifdef restrict
#   undef restrict
#endif
#include <pdal/drivers/las/Reader.hpp>
#include <pdal/PointBuffer.hpp>

#else

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

#endif


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


struct OctreeNode
{
    OctreeNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    size_t beginIndex;       ///< Begin index of points in this node
    size_t endIndex;         ///< End index of points in this node
    mutable size_t nextBeginIndex; ///< Next index for incremental rendering
    Imath::Box3f bbox;       ///< Actual bounding box of points in node
    V3f center;              ///< Centre of the node
    float radius;            ///< Distance to centre to node edge along an axis

    OctreeNode(const V3f& center, float radius)
        : beginIndex(0), endIndex(0), nextBeginIndex(0),
        center(center), radius(radius)
    {
        for (int i = 0; i < 8; ++i)
            children[i] = 0;
    }

    size_t size() const { return endIndex - beginIndex; }

    bool isLeaf() const { return beginIndex != endIndex; }
};


struct ProgressFunc
{
    PointArray& points;
    size_t totProcessed;

    ProgressFunc(PointArray& points) : points(points), totProcessed(0) {}

    void operator()(size_t additionalProcessed)
    {
        totProcessed += additionalProcessed;
        emit points.pointsLoaded(100*totProcessed/points.size());
    }
};

/// Create an octree over the given set of points with position P
///
/// The points for consideration in the current node are the set
/// P[inds[beginIndex..endIndex]]; the tree building process sorts the inds
/// array in place so that points for the output leaf nodes are held in
/// the range P[inds[node.beginIndex, node.endIndex)]].  center is the central
/// split point for splitting children of the current node; radius is the
/// current node radius measured along one of the axes.
static OctreeNode* makeTree(int depth, size_t* inds,
                            size_t beginIndex, size_t endIndex,
                            const V3f* P, const V3f& center,
                            float radius, ProgressFunc& progressFunc)
{
    OctreeNode* node = new OctreeNode(center, radius);
    const size_t pointsPerNode = 100000;
    // Limit max depth of tree to prevent infinite recursion when
    // greater than pointsPerNode points lie at the same position in
    // space.  floats effectively have 24 bit of precision in the
    // mantissa, so there's never any point splitting more than 24 times.
    const int maxDepth = 24;
    size_t* beginPtr = inds + beginIndex;
    size_t* endPtr = inds + endIndex;
    if (endIndex - beginIndex <= pointsPerNode || depth >= maxDepth)
    {
        std::random_shuffle(beginPtr, endPtr);
        for (size_t i = beginIndex; i < endIndex; ++i)
            node->bbox.extendBy(P[inds[i]]);
        node->beginIndex = beginIndex;
        node->endIndex = endIndex;
        progressFunc(endIndex - beginIndex);
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
                                     childEndIndex, P, c, r, progressFunc);
        node->bbox.extendBy(node->children[i]->bbox);
    }
    return node;
}


//------------------------------------------------------------------------------
// PointArray implementation
PointArray::PointArray()
    : m_fileName(),
    m_npoints(0),
    m_offset(0),
    m_bbox(),
    m_centroid(0)
{ }


PointArray::~PointArray()
{ }


template<typename T>
void reorderArray(std::unique_ptr<T[]>& data, const size_t* inds, size_t size)
{
    if (!data)
        return;
    std::unique_ptr<T[]> tmpData(new T[size]);
    for (size_t i = 0; i < size; ++i)
        tmpData[i] = data[inds[i]];
    data.swap(tmpData);
}


bool PointArray::loadPointFile(QString fileName, size_t maxPointCount)
{
    QTime loadTimer;
    loadTimer.start();
    m_fileName = fileName;
    size_t totPoints = 0;
    V3d Psum(0);
    m_bbox.makeEmpty();
    if (fileName.endsWith(".las") || fileName.endsWith(".laz"))
#ifdef DISPLAZ_USE_PDAL
    {
        emit loadStepStarted("Reading file");

        // Open file
        std::unique_ptr<pdal::Stage> reader(
            new pdal::drivers::las::Reader(fileName.toAscii().constData()));
        reader->initialize();
        const pdal::Schema& schema = reader->getSchema();
        bool hasColor = bool(schema.getDimensionOptional("Red"));

        // Figure out how much to decimate the point cloud.
        totPoints = reader->getNumPoints();
        size_t decimate = totPoints == 0 ? 1 : 1 + (totPoints - 1) / maxPointCount;
        if(decimate > 1)
            tfm::printf("Decimating \"%s\" by factor of %d\n", fileName.toStdString(), decimate);
        m_npoints = (totPoints + decimate - 1) / decimate;
        const pdal::Bounds<double>& bbox = reader->getBounds();
        m_offset = V3d(0.5*(bbox.getMinimum(0) + bbox.getMaximum(0)),
                       0.5*(bbox.getMinimum(1) + bbox.getMaximum(1)),
                       0.5*(bbox.getMinimum(2) + bbox.getMaximum(2)));
        // Attempt to place all data on the same vertical scale, but allow
        // other offsets if the magnitude of z is too large (and we would
        // therefore loose noticable precision by storing the data as floats)
        if (fabs(m_offset.z) < 10000)
            m_offset.z = 0;
        // Allocate all arrays
        m_P.reset(new V3f[m_npoints]);
        m_intensity.reset(new float[m_npoints]);
        m_returnNumber.reset(new unsigned char[m_npoints]);
        m_numberOfReturns.reset(new unsigned char[m_npoints]);
        m_pointSourceId.reset(new unsigned char[m_npoints]);
        m_classification.reset(new unsigned char[m_npoints]);
        if (hasColor)
            m_color.reset(new C3f[m_npoints]);
        // Output iterators for the output arrays
        V3f* outP = m_P.get();
        float* outIntens = m_intensity.get();
        unsigned char* returnNumber = m_returnNumber.get();
        unsigned char* numReturns = m_numberOfReturns.get();
        unsigned char* pointSourceId = m_pointSourceId.get();
        unsigned char* classification = m_classification.get();
        V3f* outCol = m_color.get();
        // Read big chunks of points at a time
        pdal::PointBuffer buf(schema);
        // Cache dimensions for fast access to buffer
        const pdal::Dimension& xDim = schema.getDimension("X");
        const pdal::Dimension& yDim = schema.getDimension("Y");
        const pdal::Dimension& zDim = schema.getDimension("Z");
        const pdal::Dimension *rDim = 0, *gDim = 0, *bDim = 0;
        if (hasColor)
        {
            rDim = &schema.getDimension("Red");
            gDim = &schema.getDimension("Green");
            bDim = &schema.getDimension("Blue");
        }
        const pdal::Dimension& intensityDim       = schema.getDimension("Intensity");
        const pdal::Dimension& returnNumberDim    = schema.getDimension("ReturnNumber");
        const pdal::Dimension& numberOfReturnsDim = schema.getDimension("NumberOfReturns");
        const pdal::Dimension& pointSourceIdDim   = schema.getDimension("PointSourceId");
        const pdal::Dimension& classificationDim  = schema.getDimension("Classification");
        std::unique_ptr<pdal::StageSequentialIterator> chunkIter(
                reader->createSequentialIterator(buf));
        size_t readCount = 0;
        size_t nextDecimateBlock = 1;
        size_t nextStore = 1;
        while (size_t numRead = chunkIter->read(buf))
        {
            for (size_t i = 0; i < numRead; ++i)
            {
                ++readCount;
                V3d P = V3d(xDim.applyScaling(buf.getField<int32_t>(xDim, i)),
                            yDim.applyScaling(buf.getField<int32_t>(yDim, i)),
                            zDim.applyScaling(buf.getField<int32_t>(zDim, i)));
                m_bbox.extendBy(P);
                Psum += P;
                if(readCount < nextStore)
                    continue;
                // Store the point
                *outP++ = P - m_offset;
                *outIntens++   = buf.getField<uint16_t>(intensityDim, i);
                *returnNumber++ = buf.getField<uint8_t>(returnNumberDim, i);
                *numReturns++  = buf.getField<uint8_t>(numberOfReturnsDim, i);
                *pointSourceId++ = buf.getField<uint8_t>(pointSourceIdDim, i);
                *classification++ = buf.getField<uint8_t>(classificationDim, i);
                // Extract point RGB
                if (hasColor)
                {
                    *outCol++ = (1.0f/USHRT_MAX) *
                                C3f(rDim->applyScaling(buf.getField<uint16_t>(*rDim, i)),
                                    gDim->applyScaling(buf.getField<uint16_t>(*gDim, i)),
                                    bDim->applyScaling(buf.getField<uint16_t>(*bDim, i)));
                }
                // Figure out which point will be the next stored point.
                nextDecimateBlock += decimate;
                nextStore = nextDecimateBlock;
                if(decimate > 1)
                {
                    // Randomize selected point within block to avoid repeated patterns
                    nextStore += (qrand() % decimate);
                    if(nextDecimateBlock <= totPoints && nextStore > totPoints)
                        nextStore = totPoints;
                }
            }
            emit pointsLoaded(100*readCount/totPoints);
        }
    }
#else
    {
        LASreadOpener lasReadOpener;
#ifdef _WIN32
        // Hack: liblas doesn't like forward slashes as path separators on windows
        fileName = fileName.replace('/', '\\');
#endif
        lasReadOpener.set_file_name(fileName.toAscii().constData());
        std::unique_ptr<LASreader> lasReader(lasReadOpener.open());

        if(!lasReader)
        {
            QMessageBox::critical(0, tr("Error"),
                tr("Couldn't open file \"%1\"").arg(fileName));
            return false;
        }

        //std::ofstream dumpFile("points.txt");
        emit loadStepStarted("Reading file");
        // Figure out how much to decimate the point cloud.
        totPoints = lasReader->header.number_of_point_records;
        size_t decimate = totPoints == 0 ? 1 : 1 + (totPoints - 1) / maxPointCount;
        if(decimate > 1)
            tfm::printf("Decimating \"%s\" by factor of %d\n", fileName.toStdString(), decimate);
        m_npoints = (totPoints + decimate - 1) / decimate;
        m_offset = V3d(lasReader->header.min_x, lasReader->header.min_y, 0);
        // Attempt to place all data on the same vertical scale, but allow other
        // offsets if the magnitude of z is too large (and we would therefore loose
        // noticable precision by storing the data as floats)
        if (fabs(lasReader->header.min_z) > 10000)
            m_offset.z = lasReader->header.min_z;
        m_P.reset(new V3f[m_npoints]);
        m_intensity.reset(new float[m_npoints]);
        m_returnNumber.reset(new unsigned char[m_npoints]);
        m_numberOfReturns.reset(new unsigned char[m_npoints]);
        m_pointSourceId.reset(new unsigned char[m_npoints]);
        m_classification.reset(new unsigned char[m_npoints]);
        // Iterate over all points & pull in the data.
        V3f* outP = m_P.get();
        float* outIntens = m_intensity.get();
        unsigned char* returnNumber = m_returnNumber.get();
        unsigned char* numReturns = m_numberOfReturns.get();
        unsigned char* pointSourceId = m_pointSourceId.get();
        unsigned char* classification = m_classification.get();
        size_t readCount = 0;
        size_t nextDecimateBlock = 1;
        size_t nextStore = 1;
        if (!lasReader->read_point())
            return false;
        const LASpoint& point = lasReader->point;
        if (point.have_rgb)
            m_color.reset(new C3f[m_npoints]);
        V3f* outCol = m_color.get();
        do
        {
            // Read a point from the las file
            ++readCount;
            if(readCount % 10000 == 0)
                emit pointsLoaded(100*readCount/totPoints);
            V3d P = V3d(point.get_x(), point.get_y(), point.get_z());
            m_bbox.extendBy(P);
            Psum += P;
            //if (std::max(abs(P.x - 389661.571), (abs(P.y - 7281119.875))) < 500)
                //tfm::format(dumpFile, "%.3f %.3f %.3f\n", P.x, P.y, P.z);
            if(readCount < nextStore)
                continue;
            // Store the point
            *outP++ = P - m_offset;
            // float intens = float(point.scan_angle_rank) / 40;
            *outIntens++ = point.intensity;
            *returnNumber++ = point.return_number;
            *numReturns++ = point.number_of_returns_of_given_pulse;
            *pointSourceId++ = point.point_source_ID;
            *classification++ = point.classification;
            // Extract point RGB
            if (outCol)
                *outCol++ = (1.0f/USHRT_MAX) * C3f(point.rgb[0], point.rgb[1], point.rgb[2]);
            // Figure out which point will be the next stored point.
            nextDecimateBlock += decimate;
            nextStore = nextDecimateBlock;
            if(decimate > 1)
            {
                // Randomize selected point within block to avoid repeated patterns
                nextStore += (qrand() % decimate);
                if(nextDecimateBlock <= totPoints && nextStore > totPoints)
                    nextStore = totPoints;
            }
        }
        while(lasReader->read_point());
        lasReader->close();
    }
#endif
    else
    {
        // Assume text, xyz format
        // TODO: Make this less braindead!
        std::ifstream inFile(fileName.toStdString());
        std::vector<Imath::V3d> points;
        Imath::V3d p;
        while (inFile >> p.x >> p.y >> p.z)
        {
            points.push_back(p);
            inFile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            m_bbox.extendBy(p);
        }
        m_npoints = points.size();
        totPoints = points.size();
        // Zero points + failed to get to EOF => bad text file format
        if (totPoints == 0 && !inFile.eof())
            return false;
        if (totPoints > 0)
            m_offset = points[0];
        m_P.reset(new V3f[m_npoints]);
        for (size_t i = 0; i < m_npoints; ++i)
        {
            m_P[i] = points[i] - m_offset;
            Psum += points[i];
        }
    }
    emit pointsLoaded(100);
    if (totPoints > 0)
        m_centroid = (1.0/totPoints) * Psum;
    tfm::printf("Loaded %d of %d points from file %s in %.2f seconds\n",
                m_npoints, totPoints, fileName.toStdString(),
                loadTimer.elapsed()/1000.0);

    emit loadStepStarted("Sorting points");
    std::unique_ptr<size_t[]> inds(new size_t[m_npoints]);
    for (size_t i = 0; i < m_npoints; ++i)
        inds[i] = i;
    // Expand the bound so that it's cubic.  Not exactly sure it's required
    // here, but cubic nodes sometimes work better the points are better
    // distributed for LoD, splitting is unbiased, etc.
    Imath::Box3f rootBound(m_bbox.min - m_offset, m_bbox.max - m_offset);
    V3f diag = rootBound.size();
    float rootRadius = std::max(std::max(diag.x, diag.y), diag.z) / 2;
    ProgressFunc progressFunc(*this);
    m_rootNode.reset(makeTree(0, &inds[0], 0, m_npoints, &m_P[0],
                              rootBound.center(), rootRadius, progressFunc));

    reorderArray(m_P, inds.get(), m_npoints);
    reorderArray(m_color, inds.get(), m_npoints);
    reorderArray(m_intensity, inds.get(), m_npoints);
    reorderArray(m_returnNumber, inds.get(), m_npoints);
    reorderArray(m_numberOfReturns, inds.get(), m_npoints);
    reorderArray(m_pointSourceId, inds.get(), m_npoints);
    reorderArray(m_classification, inds.get(), m_npoints);
    return true;
}


size_t PointArray::closestPoint(const V3d& pos, const V3f& N,
                                double longitudinalScale,
                                double* distance) const
{
    return closestPointToRay(m_P.get(), m_npoints, pos - m_offset, N,
                             longitudinalScale, distance);
}


static size_t simplifiedSize(const OctreeNode* node, const V3f& cameraPos,
                             double quality, bool incrementalDraw)
{
    // Distance below which all points will be drawn for a node, at quality == 1
    // TODO: Auto-adjust this for various length scales?
    const double drawAllDist = 100;
    if (node->isLeaf())
    {
        double dist = (node->bbox.center() - cameraPos).length();
        // double diagRadius = node->radius*sqrt(3);
        double diagRadius = node->bbox.size().length()/2;
        // Subtract bucket diagonal dist, since we really want an approx
        // distance to closest point in the bucket, rather than dist to centre.
        dist = std::max(10.0, dist - diagRadius);
        double desiredFraction = std::min(1.0, quality*pow(drawAllDist/dist, 2));
        size_t chunkSize = (size_t)ceil(node->size()*desiredFraction);
        size_t ndraw = chunkSize;
        if (incrementalDraw)
        {
            ndraw = (node->nextBeginIndex >= node->endIndex) ? 0 :
                    std::min(chunkSize, node->endIndex - node->nextBeginIndex);
        }
        return ndraw;
    }
    size_t ndraw = 0;
    for (int i = 0; i < 8; ++i)
    {
        OctreeNode* n = node->children[i];
        if (n)
            ndraw += simplifiedSize(n, cameraPos, quality, incrementalDraw);
    }
    return ndraw;
}


size_t PointArray::simplifiedSize(const V3d& cameraPos, bool incrementalDraw)
{
    V3f relCamera = cameraPos - m_offset;
    return ::simplifiedSize(m_rootNode.get(), relCamera, 1.0, incrementalDraw);
}


static void drawTree(const OctreeNode* node)
{
    Imath::Box3f bbox(node->center - Imath::V3f(node->radius),
                        node->center + Imath::V3f(node->radius));
    drawBoundingBox(bbox, Imath::C3f(1));
    drawBoundingBox(node->bbox, Imath::C3f(1,0,0));
    for (int i = 0; i < 8; ++i)
    {
        OctreeNode* n = node->children[i];
        if (n)
            drawTree(n);
    }
}

void PointArray::drawTree() const
{
    ::drawTree(m_rootNode.get());
}


namespace {
template<typename T, typename ArrayT>
void enableAttrOrSetDefault(QGLShaderProgram& prog, const char* attrName, const ArrayT& nonNull, const T& defaultValue)
{
    if (nonNull)
        prog.enableAttributeArray(attrName);
    else
        prog.setAttributeValue(attrName, defaultValue);
}
}


size_t PointArray::draw(QGLShaderProgram& prog, const V3d& cameraPos,
                        double quality, bool simplify, bool incrementalDraw) const
{
    prog.enableAttributeArray("position");
    enableAttrOrSetDefault(prog, "intensity",       m_intensity,       0.0f);
    enableAttrOrSetDefault(prog, "returnNumber",     m_returnNumber,     0.0f);
    enableAttrOrSetDefault(prog, "numberOfReturns", m_numberOfReturns, 0.0f);
    enableAttrOrSetDefault(prog, "pointSourceId",   m_pointSourceId,   0.0f);
    enableAttrOrSetDefault(prog, "classification",  m_classification,  0.0f);
    enableAttrOrSetDefault(prog, "color",           m_color,  QColor(0,0,0));

    // Draw points in each bucket, with total number drawn depending on how far
    // away the bucket is.  Since the points are shuffled, this corresponds to
    // a stochastic simplification of the full point cloud.
    size_t totDrawn = 0;
    V3f relCamera = cameraPos - m_offset;
    std::vector<const OctreeNode*> nodeStack;
    nodeStack.push_back(m_rootNode.get());
    while (!nodeStack.empty())
    {
        const OctreeNode* node = nodeStack.back();
        nodeStack.pop_back();
        if (!node->isLeaf())
        {
            for (int i = 0; i < 8; ++i)
            {
                OctreeNode* n = node->children[i];
                if (n)
                    nodeStack.push_back(n);
            }
            continue;
        }
        size_t ndraw = node->size();
        double lodMultiplier = 1;
        size_t idx = node->beginIndex;
        if (!incrementalDraw)
            node->nextBeginIndex = node->beginIndex;
        if (simplify)
        {
            ndraw = ::simplifiedSize(node, relCamera, quality, incrementalDraw);
            idx = node->nextBeginIndex;
            if (ndraw == 0)
                continue;
            // For LoD, compute the desired fraction of points for this node.
            //
            // The desired fraction is chosen such that the density of points
            // per solid angle is constant; when removing points, the point
            // radii are scaled up to keep the total area covered by the points
            // constant.
            //lodMultiplier = sqrt(double(node->size())/ndraw);
        }
        prog.setAttributeArray("position",  (const GLfloat*)(m_P.get() + idx), 3);
        if (m_intensity)       prog.setAttributeArray("intensity", m_intensity.get() + idx,           1);
        if (m_returnNumber)     prog.setAttributeArray("returnNumber",     GL_UNSIGNED_BYTE, m_returnNumber.get()     + idx, 1);
        if (m_numberOfReturns) prog.setAttributeArray("numberOfReturns", GL_UNSIGNED_BYTE, m_numberOfReturns.get() + idx, 1);
        if (m_pointSourceId)   prog.setAttributeArray("pointSourceId",   GL_UNSIGNED_BYTE, m_pointSourceId.get()   + idx, 1);
        if (m_classification)  prog.setAttributeArray("classification",  GL_UNSIGNED_BYTE, m_classification.get()  + idx, 1);
        if (m_color)           prog.setAttributeArray("color", (const GLfloat*)(m_color.get() + idx), 3);
        prog.setUniformValue("pointSizeLodMultiplier", (GLfloat)lodMultiplier);
        glDrawArrays(GL_POINTS, 0, ndraw);
        node->nextBeginIndex += ndraw;
        totDrawn += ndraw;
    }
    //tfm::printf("Drew %d of total points %d, quality %f\n", totDraw, m_npoints, quality);

    // Disable all attribute arrays - leaving these enabled seems to screw with
    // the OpenGL fixed function pipeline in unusual ways.
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("intensity");
    prog.disableAttributeArray("returnNumber");
    prog.disableAttributeArray("numberOfReturns");
    prog.disableAttributeArray("pointSourceId");
    prog.disableAttributeArray("classification");
    prog.disableAttributeArray("color");
    return totDrawn;
}


