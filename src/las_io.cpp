// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "util.h"
#include "QtLogger.h"
#include "PointArray.h"

#include <random>

// FIXME! The point loader code is really a horrible mess.  It should be
// cleaned up to conform to a proper interface, and moved out of the PointArray
// class completely.

#ifndef DISPLAZ_USE_LAS

bool PointArray::loadLas(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, uint64_t& totalPoints)
{
    g_logger.error("Cannot load %s: Displaz built without las support!", fileName);
    return false;
}

#else // DISPLAZ_USE_LAS

// Use laslib
#ifdef _MSC_VER
#   pragma warning(push)
#   pragma warning(disable : 4996)
#   pragma warning(disable : 4267)
#elif __GNUC__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wstrict-aliasing"
#   ifndef __clang__
#   pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#   endif
#endif
// Note... laslib generates a small horde of warnings
#include <lasreader_las.hpp>
#ifdef _MSC_VER
#   pragma warning(push)
#elif __GNUC__
#   pragma GCC diagnostic pop
#endif



bool PointArray::loadLas(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, uint64_t& totalPoints)
{
    File file;
    auto lasReadOpener = std::make_unique<LASreadOpener>();
    auto lasReader = std::make_unique<LASreaderLAS>(lasReadOpener.get());
#ifdef _WIN32
    file = _wfopen(fileName.toStdWString().data(), L"rb");
#else
    file = fopen(fileName.toUtf8().constData(), "rb");
#endif
    if(!file || !lasReader->open(file))
    {
        g_logger.error("Couldn't open file \"%s\"", fileName);
        return false;
    }

    //std::ofstream dumpFile("points.txt");
    // Figure out how much to decimate the point cloud.
    totalPoints = std::max<uint64_t>(lasReader->header.extended_number_of_point_records,
                                   lasReader->header.number_of_point_records);
    size_t decimate = totalPoints == 0 ? 1 : 1 + (totalPoints - 1) / maxPointCount;
    if(decimate > 1)
    {
        g_logger.info("Decimating \"%s\" by factor of %d",
                        fileName.toStdString(), decimate);
    }
    npoints = (totalPoints + decimate - 1) / decimate;
    offset = V3d(lasReader->header.x_offset, lasReader->header.y_offset, lasReader->header.z_offset);

    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    fields.push_back(GeomField(TypeSpec::uint16_i(), "intensity", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "returnNumber", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "numberOfReturns", npoints));
    fields.push_back(GeomField(TypeSpec::uint16_i(), "pointSourceId", npoints));
    fields.push_back(GeomField(TypeSpec::uint8_i(), "classification", npoints));
    if (totalPoints == 0)
    {
        g_logger.warning("File %s has zero points", fileName);
        return true;
    }
    // Iterate over all points & pull in the data.
    V3f* position           = (V3f*)fields[0].as<float>();
    uint16_t* intensity     = fields[1].as<uint16_t>();
    uint8_t* returnNumber   = fields[2].as<uint8_t>();
    uint8_t* numReturns     = fields[3].as<uint8_t>();
    uint16_t* pointSourceId  = fields[4].as<uint16_t>();
    uint8_t* classification = fields[5].as<uint8_t>();
    uint64_t readCount = 0;
    uint64_t nextDecimateBlock = 1;
    uint64_t nextStore = 1;
    size_t storeCount = 0;
    std::mt19937 rand;
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
            emit loadProgress(100*readCount/totalPoints);
        V3d P = V3d(point.get_x(), point.get_y(), point.get_z());
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

        if (point.extended_point_type) {
            *classification++ = point.extended_classification;
        } else {
            // Put flags back in classification byte to avoid memory bloat
            *classification++ = point.classification | (point.synthetic_flag << 5) |
                                (point.keypoint_flag << 6) | (point.withheld_flag << 7);
        }

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
            if(nextDecimateBlock <= totalPoints && nextStore > totalPoints)
                nextStore = totalPoints;
        }
    }
    while(lasReader->read_point());
    lasReader->close();
    if (readCount < totalPoints)
    {
        g_logger.warning("Expected %d points in file \"%s\", got %d",
                         totalPoints, fileName, readCount);
        npoints = storeCount;
        // Shrink all fields to fit - these will have wasted space at the end,
        // but that will be fixed during reordering.
        for (size_t i = 0; i < fields.size(); ++i)
            fields[i].size = npoints;
        totalPoints = readCount;
    }
    return true;
}


#endif // DISPLAZ_USE_LAS
