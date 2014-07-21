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

#include "qtlogger.h"
#include "pointarray.h"

// FIXME! The point loader code is really a horrible mess.  It should be
// cleaned up to conform to a proper interface, and moved out of the PointArray
// class completely.


#ifndef DISPLAZ_USE_LAS

bool PointArray::loadLas(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, size_t& totPoints,
                         Imath::Box3d& bbox, V3d& centroid)
{
    g_logger.error("Cannot load %s: Displaz built without las support!", fileName);
    return false;
}

#else // DISPLAZ_USE_LAS

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



bool PointArray::loadLas(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, size_t& totPoints,
                         Imath::Box3d& bbox, V3d& centroid)
{
    V3d Psum(0);
#ifdef DISPLAZ_USE_PDAL
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
    {
        g_logger.info("Decimating \"%s\" by factor of %d",
                      fileName.toStdString(), decimate);
    }
    npoints = (totPoints + decimate - 1) / decimate;
    const pdal::Bounds<double>& bbox = reader->getBounds();
    offset = V3d(0.5*(bbox.getMinimum(0) + bbox.getMaximum(0)),
                 0.5*(bbox.getMinimum(1) + bbox.getMaximum(1)),
                 0.5*(bbox.getMinimum(2) + bbox.getMaximum(2)));
    // Attempt to place all data on the same vertical scale, but allow
    // other offsets if the magnitude of z is too large (and we would
    // therefore loose noticable precision by storing the data as floats)
    if (fabs(offset.z) < 10000)
        offset.z = 0;
    // Allocate all arrays
    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    fields.push_back(GeomField(TypeSpec::uint16_i(), "intensity", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "returnNumber", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "numberOfReturns", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "pointSourceId", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "classification", npoints));
    // Output iterators for the output arrays
    V3f* position           = (V3f*)fields[0].as<float>();
    uint16_t* intensity     = fields[1].as<uint16_t>();
    uint8_t* returnNumber   = fields[2].as<uint8_t>();
    uint8_t* numReturns     = fields[3].as<uint8_t>();
    uint8_t* pointSourceId  = fields[4].as<uint8_t>();
    uint8_t* classification = fields[5].as<uint8_t>();
    uint16_t* color = 0;
    if (hasColor)
    {
        fields.push_back(GeomField(TypeSpec(TypeSpec::Uint,2,3,TypeSpec::Color),
                                        "color", npoints));
        color = fields.back().as<uint16_t>();
    }
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
    size_t storeCount = 0;
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
            bbox.extendBy(P);
            Psum += P;
            if(readCount < nextStore)
                continue;
            ++storeCount;
            // Store the point
            *position++ = P - offset;
            *intensity++   = buf.getField<uint16_t>(intensityDim, i);
            *returnNumber++ = buf.getField<uint8_t>(returnNumberDim, i);
            *numReturns++  = buf.getField<uint8_t>(numberOfReturnsDim, i);
            *pointSourceId++ = buf.getField<uint8_t>(pointSourceIdDim, i);
            *classification++ = buf.getField<uint8_t>(classificationDim, i);
            // Extract point RGB
            if (hasColor)
            {
                *color++ = buf.getField<uint16_t>(*rDim, i);
                *color++ = buf.getField<uint16_t>(*gDim, i);
                *color++ = buf.getField<uint16_t>(*bDim, i);
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
        emit loadProgress(100*readCount/totPoints);
    }
#else
    LASreadOpener lasReadOpener;
#ifdef _WIN32
    // Hack: liblas doesn't like forward slashes as path separators on windows
    fileName = fileName.replace('/', '\\');
#endif
    lasReadOpener.set_file_name(fileName.toAscii().constData());
    std::unique_ptr<LASreader> lasReader(lasReadOpener.open());

    if(!lasReader)
    {
        g_logger.error("Couldn't open file \"%s\"", fileName);
        return false;
    }

    //std::ofstream dumpFile("points.txt");
    // Figure out how much to decimate the point cloud.
    totPoints = lasReader->header.number_of_point_records;
    size_t decimate = totPoints == 0 ? 1 : 1 + (totPoints - 1) / maxPointCount;
    if(decimate > 1)
    {
        g_logger.info("Decimating \"%s\" by factor of %d",
                        fileName.toStdString(), decimate);
    }
    npoints = (totPoints + decimate - 1) / decimate;
    offset = V3d(lasReader->header.min_x, lasReader->header.min_y, 0);
    // Attempt to place all data on the same vertical scale, but allow other
    // offsets if the magnitude of z is too large (and we would therefore loose
    // noticable precision by storing the data as floats)
    if (fabs(lasReader->header.min_z) > 10000)
        offset.z = lasReader->header.min_z;
    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    fields.push_back(GeomField(TypeSpec::uint16_i(), "intensity", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "returnNumber", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "numberOfReturns", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "pointSourceId", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "classification", npoints));
    if (totPoints == 0)
    {
        g_logger.warning("File %s has zero points", fileName);
        return true;
    }
    // Iterate over all points & pull in the data.
    V3f* position           = (V3f*)fields[0].as<float>();
    uint16_t* intensity     = fields[1].as<uint16_t>();
    uint8_t* returnNumber   = fields[2].as<uint8_t>();
    uint8_t* numReturns     = fields[3].as<uint8_t>();
    uint8_t* pointSourceId  = fields[4].as<uint8_t>();
    uint8_t* classification = fields[5].as<uint8_t>();
    size_t readCount = 0;
    size_t storeCount = 0;
    size_t nextDecimateBlock = 1;
    size_t nextStore = 1;
    if (!lasReader->read_point())
        return false;
    const LASpoint& point = lasReader->point;
    uint16_t* color = 0;
    if (point.have_rgb)
    {
        fields.push_back(GeomField(TypeSpec(TypeSpec::Uint,2,3,TypeSpec::Color),
                                        "color", npoints));
        color = fields.back().as<uint16_t>();
    }
    do
    {
        // Read a point from the las file
        ++readCount;
        if(readCount % 10000 == 0)
            emit loadProgress(100*readCount/totPoints);
        V3d P = V3d(point.get_x(), point.get_y(), point.get_z());
        bbox.extendBy(P);
        Psum += P;
        if(readCount < nextStore)
            continue;
        ++storeCount;
        // Store the point
        *position++ = P - offset;
        // float intens = float(point.scan_angle_rank) / 40;
        *intensity++ = point.intensity;
        *returnNumber++ = point.return_number;
#       if LAS_TOOLS_VERSION >= 140315
        *numReturns++ = point.number_of_returns;
#       else
        *numReturns++ = point.number_of_returns_of_given_pulse;
#       endif
        *pointSourceId++ = point.point_source_ID;
        *classification++ = point.classification;
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
            nextStore += (qrand() % decimate);
            if(nextDecimateBlock <= totPoints && nextStore > totPoints)
                nextStore = totPoints;
        }
    }
    while(lasReader->read_point());
    lasReader->close();
#endif
    if (readCount < totPoints)
    {
        g_logger.warning("Expected %d points in file \"%s\", got %d",
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
    return true;
}


#endif // DISPLAZ_USE_LAS
