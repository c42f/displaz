// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "qtlogger.h"
#include "pointarray.h"

// FIXME! The point loader code is really a horrible mess.  It should be
// cleaned up to conform to a proper interface, and moved out of the PointArray
// class completely.


#ifndef DISPLAZ_USE_LAS

bool PointArray::loadLas(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, uint64_t& totalPoints,
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
#include <pdal/Stage.hpp>
#include <pdal/StageFactory.hpp>
#include <pdal/util/FileUtils.hpp>
#include <pdal/PipelineReader.hpp>
#include <pdal/PipelineManager.hpp>

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
                         size_t& npoints, uint64_t& totalPoints,
                         Imath::Box3d& bbox, V3d& centroid)
{
    V3d Psum(0);
#ifdef DISPLAZ_USE_PDAL
    // Open file
    if (!pdal::FileUtils::fileExists(fileName.toAscii().constData()))
    {
        g_logger.info("File \"%s\" does not exist", fileName.toStdString() );
        return false;
    }
    std::unique_ptr<pdal::PipelineManager> manager(new pdal::PipelineManager);

    pdal::StageFactory factory;
    std::string driver = factory.inferReaderDriver(fileName.toStdString());
    manager->addReader(driver);

    pdal::Stage* reader = static_cast<pdal::Reader*>(manager->getStage());
    pdal::Options options;
    pdal::Option fname("filename", fileName.toStdString());
    options.add(fname);
    reader->setOptions(options);

    manager->execute();
    pdal::PointBufferSet pbSet = manager->buffers();

    pdal::PointContext context = manager->context();
    bool hasColor = context.hasDim(pdal::Dimension::Id::Red);
    pdal::QuickInfo quickinfo = reader->preview();

    // Figure out how much to decimate the point cloud.
    totalPoints = quickinfo.m_pointCount;
    size_t decimate = totalPoints == 0 ? 1 : 1 + (totalPoints - 1) / maxPointCount;
    if(decimate > 1)
    {
        g_logger.info("Decimating \"%s\" by factor of %d",
                      fileName.toStdString(), decimate);
    }
    npoints = (totalPoints + decimate - 1) / decimate;
    pdal::BOX3D pdal_bounds = quickinfo.m_bounds;
    offset = V3d(0.5*(pdal_bounds.minx + pdal_bounds.maxx),
                 0.5*(pdal_bounds.miny + pdal_bounds.maxy),
                 0.5*(pdal_bounds.minz + pdal_bounds.maxz));
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
    size_t readCount = 0;
    size_t storeCount = 0;
    size_t nextDecimateBlock = 1;
    size_t nextStore = 1;
    for (auto st = pbSet.begin(); st != pbSet.end(); ++st)
//     while (size_t numRead = chunkIter->read(buf))
    {
        pdal::PointBufferPtr buf = *st;
        for (size_t i = 0; i < buf->size(); ++i)
        {
            ++readCount;
            V3d P = V3d(buf->getFieldAs<double>(pdal::Dimension::Id::X, i),
                        buf->getFieldAs<double>(pdal::Dimension::Id::Y, i),
                        buf->getFieldAs<double>(pdal::Dimension::Id::Z, i));
//             V3d P = V3d(xDim.applyScaling(buf.getField<int32_t>(xDim, i)),
//                         yDim.applyScaling(buf.getField<int32_t>(yDim, i)),
//                         zDim.applyScaling(buf.getField<int32_t>(zDim, i)));
            bbox.extendBy(P);
            Psum += P;
            if(readCount < nextStore)
                continue;
            ++storeCount;
            // Store the point
            *position++ = P - offset;
            *intensity++   = buf->getFieldAs<uint16_t>(pdal::Dimension::Id::Intensity, i);
            *returnNumber++ = buf->getFieldAs<uint8_t>(pdal::Dimension::Id::ReturnNumber, i);
            *numReturns++  = buf->getFieldAs<uint8_t>(pdal::Dimension::Id::NumberOfReturns, i);
            *pointSourceId++ = buf->getFieldAs<uint8_t>(pdal::Dimension::Id::PointSourceId, i);
            *classification++ = buf->getFieldAs<uint8_t>(pdal::Dimension::Id::Classification, i);
            // Extract point RGB
            if (hasColor)
            {
                *color++ = buf->getFieldAs<uint16_t>(pdal::Dimension::Id::Red, i);
                *color++ = buf->getFieldAs<uint16_t>(pdal::Dimension::Id::Green, i);
                *color++ = buf->getFieldAs<uint16_t>(pdal::Dimension::Id::Blue, i);
            }
            // Figure out which point will be the next stored point.
            nextDecimateBlock += decimate;
            nextStore = nextDecimateBlock;
            if(decimate > 1)
            {
                // Randomize selected point within block to avoid repeated patterns
                nextStore += (qrand() % decimate);
                if(nextDecimateBlock <= totalPoints && nextStore > totalPoints)
                    nextStore = totalPoints;
            }
        }
        emit loadProgress(100*readCount/totalPoints);
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
    totalPoints = std::max<uint64_t>(lasReader->header.extended_number_of_point_records,
                                   lasReader->header.number_of_point_records);
    size_t decimate = totalPoints == 0 ? 1 : 1 + (totalPoints - 1) / maxPointCount;
    if(decimate > 1)
    {
        g_logger.info("Decimating \"%s\" by factor of %d",
                        fileName.toStdString(), decimate);
    }
    npoints = (totalPoints + decimate - 1) / decimate;
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
    uint8_t* pointSourceId  = fields[4].as<uint8_t>();
    uint8_t* classification = fields[5].as<uint8_t>();
    uint64_t readCount = 0;
    uint64_t nextDecimateBlock = 1;
    uint64_t nextStore = 1;
    size_t storeCount = 0;
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
        // Put flags back in classification byte to avoid memory bloat
        *classification++ = point.classification | (point.synthetic_flag << 5) |
                            (point.keypoint_flag << 6) | (point.withheld_flag << 7);
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
            if(nextDecimateBlock <= totalPoints && nextStore > totalPoints)
                nextStore = totalPoints;
        }
    }
    while(lasReader->read_point());
    lasReader->close();
#endif
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
    if (totalPoints > 0)
        centroid = (1.0/totalPoints)*Psum;
    return true;
}


#endif // DISPLAZ_USE_LAS
