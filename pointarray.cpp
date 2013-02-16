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
// Hack: kill gcc unused variable warning
class MonkeyChops { MonkeyChops() { (void)LAS_TOOLS_FORMAT_NAMES; } };
#endif


//------------------------------------------------------------------------------
// PointArray implementation

PointArray::PointArray()
    : m_npoints(0)
{ }


bool PointArray::loadPointFile(QString fileName, size_t maxPointCount)
{
    m_fileName = fileName;
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

    // Figure out how much to decimate the point cloud.
    size_t totPoints = lasReader->header.number_of_point_records;
    size_t decimate = (totPoints + maxPointCount - 1) / maxPointCount;
    if(decimate > 1)
        tfm::printf("Decimating \"%s\" by factor of %d\n", fileName.toStdString(), decimate);
    m_npoints = (totPoints + decimate - 1) / decimate;
    m_offset = V3d(lasReader->header.min_x, lasReader->header.min_y,
                          lasReader->header.min_z);
    m_P.reset(new V3f[m_npoints]);
    m_color.reset(new C3f[m_npoints]);
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
    V3d Psum(0);
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
    emit pointsLoaded(100);
    m_centroid = (1.0/totPoints) * Psum;
    lasReader->close();
    tfm::printf("Displaying %d of %d points from file %s\n", m_npoints,
                totPoints, fileName.toStdString());
    return true;
}


size_t PointArray::closestPoint(V3d pos, double* distance) const
{
    pos -= m_offset;
    const V3f* P = m_P.get();
    size_t nearestIdx = -1;
    double nearestDist = DBL_MAX;
    for(size_t i = 0; i < m_npoints; ++i, ++P)
    {
        float dist = (pos - *P).length2();
        if(dist < nearestDist)
        {
            nearestDist = dist;
            nearestIdx = i;
        }
    }
    if(distance)
        *distance = nearestDist;
    return nearestIdx;
}


void PointArray::draw(QGLShaderProgram& prog, const V3d& cameraPos) const
{
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("intensity");
    prog.enableAttributeArray("returnIndex");
    prog.enableAttributeArray("numberOfReturns");
    prog.enableAttributeArray("pointSourceId");
    if (m_color)
        prog.enableAttributeArray("color");
    size_t chunkSize = 1000000;
    for (size_t i = 0; i < m_npoints; i += chunkSize)
    {
        prog.setAttributeArray("intensity", m_intensity.get() + i, 1);
        prog.setAttributeArray("position", (const GLfloat*)(m_P.get() + i), 3);
        prog.setAttributeArray("returnIndex", GL_UNSIGNED_BYTE, m_returnIndex.get() + i, 1);
        prog.setAttributeArray("numberOfReturns", GL_UNSIGNED_BYTE, m_numberOfReturns.get() + i, 1);
        prog.setAttributeArray("pointSourceId", GL_UNSIGNED_BYTE, m_pointSourceId.get() + i, 1);
        if (m_color)
            prog.setAttributeArray("color", (const GLfloat*)(m_color.get() + i), 3);
        int ndraw = (int)std::min(m_npoints - i, chunkSize);
        glDrawArrays(GL_POINTS, 0, ndraw);
    }
}


