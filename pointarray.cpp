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

#include <QtGui/QMessageBox>
#include <QtOpenGL/QGLShaderProgram>

#include <unordered_map>
#include <fstream>

#include "tinyformat.h"

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


//------------------------------------------------------------------------------
// Hashing of std::pair, copied from stack overflow
template <class T>
inline void hash_combine(std::size_t & seed, const T & v)
{
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
  template<typename S, typename T> struct hash<pair<S, T>>
  {
    inline size_t operator()(const pair<S, T> & v) const
    {
      size_t seed = 0;
      ::hash_combine(seed, v.first);
      ::hash_combine(seed, v.second);
      return seed;
    }
  };
}


//------------------------------------------------------------------------------
// PointArray implementation

PointArray::PointArray()
    : m_fileName(),
    m_npoints(0),
    m_bucketWidth(100),
    m_offset(0),
    m_bbox(),
    m_centroid(0),
    m_buckets()
{ }

PointArray::~PointArray()
{ }

struct PointArray::Bucket
{
    V3f centroid;
    size_t beginIndex;
    size_t endIndex;

    Bucket(V3f centroid, size_t beginIndex, size_t endIndex)
        : centroid(centroid),
        beginIndex(beginIndex),
        endIndex(endIndex)
    { }
};


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
    m_fileName = fileName;
    size_t totPoints = 0;
    V3d Psum(0);
    if (fileName.endsWith(".las") || fileName.endsWith(".laz"))
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
        m_returnIndex.reset(new unsigned char[m_npoints]);
        m_numberOfReturns.reset(new unsigned char[m_npoints]);
        m_pointSourceId.reset(new unsigned char[m_npoints]);
        m_bbox.makeEmpty();
        // Iterate over all points & pull in the data.
        V3f* outP = m_P.get();
        float* outIntens = m_intensity.get();
        unsigned char* returnIndex = m_returnIndex.get();
        unsigned char* numReturns = m_numberOfReturns.get();
        unsigned char* pointSourceId = m_pointSourceId.get();
        size_t readCount = 0;
        size_t nextBlock = 1;
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
            if(readCount < nextStore)
                continue;
            // Store the point
            *outP++ = P - m_offset;
            // float intens = float(point.scan_angle_rank) / 40;
            *outIntens++ = point.intensity;
            *returnIndex++ = point.return_number;
            *numReturns++ = point.number_of_returns_of_given_pulse;
            *pointSourceId++ = point.point_source_ID;
            // Extract point RGB
            if (outCol)
                *outCol++ = (1.0f/USHRT_MAX) * C3f(point.rgb[0], point.rgb[1], point.rgb[2]);
            // Figure out which point will be the next stored point.
            nextBlock += decimate;
            nextStore = nextBlock;
            if(decimate > 1)
            {
                // Randomize selected point within block to avoid repeated patterns
                nextStore += (qrand() % decimate);
                if(nextBlock <= totPoints && nextStore > totPoints)
                    nextStore = totPoints;
            }
        }
        while(lasReader->read_point());
        lasReader->close();
    }
    else
    {
        // Assume text, xyz format
        // TODO: Make this less braindead!
        std::ifstream inFile(fileName.toStdString());
        std::vector<Imath::V3d> points;
        Imath::V3d p;
        m_bbox.makeEmpty();
        while (inFile >> p.x >> p.y >> p.z)
        {
            points.push_back(p);
            m_bbox.extendBy(p);
        }
        m_npoints = points.size();
        totPoints = points.size();
        m_offset = points[0];
        m_P.reset(new V3f[m_npoints]);
        m_intensity.reset(new float[m_npoints]);
        m_returnIndex.reset(new unsigned char[m_npoints]);
        m_numberOfReturns.reset(new unsigned char[m_npoints]);
        m_pointSourceId.reset(new unsigned char[m_npoints]);
        for (size_t i = 0; i < m_npoints; ++i)
        {
            m_P[i] = points[i] - m_offset;
            Psum += points[i];
            m_intensity[i] = 0;
        }
    }
    emit pointsLoaded(100);
    m_centroid = (1.0/totPoints) * Psum;
    tfm::printf("Displaying %d of %d points from file %s\n", m_npoints,
                totPoints, fileName.toStdString());

    emit loadStepStarted("Bucketing points");
    typedef std::unordered_map<std::pair<int,int>, std::vector<size_t> > IndexMap;
    float invBucketSize = 1/m_bucketWidth;
    IndexMap buckets;
    for (size_t i = 0; i < m_npoints; ++i)
    {
        int x = (int)floor(m_P[i].x*invBucketSize);
        int y = (int)floor(m_P[i].y*invBucketSize);
        buckets[std::make_pair(x,y)].push_back(i);
        if(i % 100000 == 0)
            emit pointsLoaded(100*i/m_npoints);
    }

    //emit loadStepStarted("Shuffling buckets");
    std::unique_ptr<size_t[]> inds(new size_t[m_npoints]);
    size_t idx = 0;
    m_buckets.clear();
    m_buckets.reserve(buckets.size() + 1);
    for (IndexMap::iterator b = buckets.begin(); b != buckets.end(); ++b)
    {
        size_t startIdx = idx;
        std::vector<size_t>& bucketInds = b->second;
        V3f centroid(0);
        // Random shuffle so that choosing an initial sequence will correspond
        // to a stochastic simplification of the bucket.  TODO: Use a
        // quasirandom shuffling method for better visual properties.
        std::random_shuffle(bucketInds.begin(), bucketInds.end());
        for (size_t i = 0, iend = bucketInds.size(); i < iend; ++i, ++idx)
        {
            inds[idx] = bucketInds[i];
            centroid += m_P[inds[idx]];
        }
        centroid *= 1.0f/(idx - startIdx);
        m_buckets.push_back(Bucket(centroid, startIdx, idx));
    }
    reorderArray(m_P, inds.get(), m_npoints);
    reorderArray(m_color, inds.get(), m_npoints);
    reorderArray(m_intensity, inds.get(), m_npoints);
    reorderArray(m_returnIndex, inds.get(), m_npoints);
    reorderArray(m_numberOfReturns, inds.get(), m_npoints);
    reorderArray(m_pointSourceId, inds.get(), m_npoints);
    return true;
}


size_t PointArray::closestPoint(V3d pos, V3f N,
                                double normalDirectionScale,
                                double* distance) const
{
    pos -= m_offset;
    N.normalize();
    const V3f* P = m_P.get();
    size_t nearestIdx = -1;
    double nearestDist2 = DBL_MAX;
    double f = normalDirectionScale*normalDirectionScale;
    for(size_t i = 0; i < m_npoints; ++i, ++P)
    {
        V3f v = pos - *P;
        float distN = N.dot(v);
        float distNperp = (v - distN*N).length2();
        float d = f*distN*distN + distNperp;
        //float d = (pos - *P).length2();
        if(d < nearestDist2)
        {
            nearestDist2 = d;
            nearestIdx = i;
        }
    }
    if(distance)
        *distance = sqrt(nearestDist2);
    return nearestIdx;
}


void PointArray::draw(QGLShaderProgram& prog, const V3d& cameraPos,
                      double quality) const
{
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("intensity");
    prog.enableAttributeArray("returnIndex");
    prog.enableAttributeArray("numberOfReturns");
    prog.enableAttributeArray("pointSourceId");
    if (m_color)
        prog.enableAttributeArray("color");
    else
    {
        prog.setAttributeValue("color", 0.0f, 0.0f, 0.0f);
    }

    // Draw points in each bucket, with total number drawn depending on how far
    // away the bucket is.  Since the points are shuffled, this corresponds to
    // a stochastic simplification of the full point cloud.
    size_t totDraw = 0;
    for (size_t bucketIdx = 0; bucketIdx < m_buckets.size(); ++bucketIdx)
    {
        const Bucket& bucket = m_buckets[bucketIdx];
        size_t b = bucket.beginIndex;
        prog.setAttributeArray("position",  (const GLfloat*)(m_P.get() + b), 3);
        prog.setAttributeArray("intensity", m_intensity.get() + b,           1);
        prog.setAttributeArray("returnIndex",     GL_UNSIGNED_BYTE, m_returnIndex.get()     + b, 1);
        prog.setAttributeArray("numberOfReturns", GL_UNSIGNED_BYTE, m_numberOfReturns.get() + b, 1);
        prog.setAttributeArray("pointSourceId",   GL_UNSIGNED_BYTE, m_pointSourceId.get()   + b, 1);
        if (m_color)
            prog.setAttributeArray("color", (const GLfloat*)(m_color.get() + b), 3);
        // Compute the desired fraction of points for this bucket.
        //
        // The desired fraction is chosen such that the density of points per
        // solid angle is constant; when removing points, the point radii are
        // scaled up to keep the total area covered by the points constant.
        float dist = (bucket.centroid + m_offset - cameraPos).length();
        double desiredFraction = quality <= 0 ? 1 : quality * pow(m_bucketWidth/dist, 2);
        size_t ndraw = bucket.endIndex - bucket.beginIndex;
        float lodMultiplier = 1;
        if (desiredFraction < 1)
        {
            ndraw = std::max((size_t) 1, (size_t) (ndraw*desiredFraction));
            lodMultiplier = (float)sqrt(1/desiredFraction);
        }
        prog.setUniformValue("pointSizeLodMultiplier", lodMultiplier);
        glDrawArrays(GL_POINTS, 0, ndraw);
        totDraw += ndraw;
    }
    //tfm::printf("Drew %.2f%% of total points\n", 100.0*totDraw/m_npoints);

    // Disable all attribute arrays - leaving these enabled seems to screw with
    // the OpenGL fixed function pipeline in unusual ways.
    prog.disableAttributeArray("position");
    prog.disableAttributeArray("intensity");
    prog.disableAttributeArray("returnIndex");
    prog.disableAttributeArray("numberOfReturns");
    prog.disableAttributeArray("pointSourceId");
    prog.disableAttributeArray("color");
}


