// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "pointdbwriter.h"

#include <algorithm>
#include <fstream>
#include <memory>

#include <QString>
#include <QFileInfo>
#include <QDir>

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

struct PointDbWriter::PointDbTile
{
    PointDbTile(TilePos tilePos) : tilePos(tilePos), recentlyUsed(false) {}

    TilePos tilePos;
    std::vector<float> position;
    std::vector<float> intensity;

    bool recentlyUsed;

    size_t numPoints() const { return position.size()/3; }

    size_t sizeBytes() const
    {
        return sizeof(float)*position.capacity() +
               sizeof(float)*intensity.capacity();
    }

    bool empty() const { return position.empty(); }
};


PointDbWriter::PointDbWriter(const std::string& dirName, const Imath::Box3d& boundingBox,
                             double tileSize, size_t flushInterval, Logger& logger)
    : m_dirName(dirName),
    m_boundingBox(boundingBox),
    m_tileSize(tileSize),
    m_offset(0),
    m_computeBounds(boundingBox.isEmpty()),
    m_flushInterval(flushInterval),
    m_haveOffset(false),
    m_prevTile(nullptr),
    m_pointsWritten(0),
    m_logger(logger)
{
    QString qdirName = QString::fromUtf8(dirName.c_str(), dirName.size());
    if (QFileInfo(qdirName).isDir())
        throw DisplazError("Point output directory already exists: %s", dirName);
    if (!QDir().mkpath(qdirName))
        throw DisplazError("Could not create directory: %s", dirName);
}


size_t PointDbWriter::cacheSizeBytes() const
{
    size_t bytes = 0;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
        bytes += it->second.sizeBytes();
    return bytes;
}


void PointDbWriter::writePoint(Imath::V3d P, float intensity)
{
    if (!m_haveOffset)
    {
        m_offset = P;
        m_haveOffset = true;
    }
    TilePos tilePos((int)floor(P.x/m_tileSize),
                    (int)floor(P.y/m_tileSize),
                    (int)floor(P.z/m_tileSize));
    PointDbTile& tile = findTile(tilePos);
    if (m_computeBounds)
        m_boundingBox.extendBy(P);
    assert(m_boundingBox.intersects(P));
    tile.position.push_back(P.x - m_offset.x);
    tile.position.push_back(P.y - m_offset.y);
    tile.position.push_back(P.z - m_offset.z);
    tile.intensity.push_back(intensity);
    m_pointsWritten += 1;
    if (m_pointsWritten % m_flushInterval == 0)
        flushTiles();
}


void PointDbWriter::close()
{
    flushTiles(true);
    // Write config file
    std::ofstream dbConfig(tfm::format("%s/config.txt", m_dirName));
    tfm::format(dbConfig,
        "%.17e\n"
        "%.17e %.17e %.17e %.17e %.17e %.17e\n"
        "%.17e %.17e %.17e\n",
        m_tileSize,
        m_boundingBox.min.x, m_boundingBox.min.y, m_boundingBox.min.z,
        m_boundingBox.max.x, m_boundingBox.max.y, m_boundingBox.max.z,
        m_offset.x, m_offset.y, m_offset.z
    );

    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
    {
        tfm::format(dbConfig, "%d %d %d\n", it->second.tilePos.x,
                    it->second.tilePos.y, it->second.tilePos.z);
    }
}


PointDbWriter::PointDbTile& PointDbWriter::findTile(const TilePos& pos)
{
    if (m_prevTile && m_prevTile->tilePos == pos)
    {
        m_prevTile->recentlyUsed = true;
        return *m_prevTile;
    }
    auto it = m_cache.find(pos);
    if (it == m_cache.end())
    {
        // Create new empty tile
        it = m_cache.insert(std::make_pair(pos, PointDbTile(pos))).first;
    }
    it->second.recentlyUsed = true;
    m_prevTile = &it->second;
    return it->second;
}


void PointDbWriter::flushTiles(bool forceFlushAll)
{
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
    {
        PointDbTile& tile = it->second;
        if ((forceFlushAll || !tile.recentlyUsed) && !tile.empty())
            flushToDisk(tile);
        tile.recentlyUsed = false;
    }
}


void PointDbWriter::flushToDisk(PointDbTile& tile)
{
    assert(!tile.empty());
    std::string fileName = tfm::format("%s/%d_%d_%d.dat", m_dirName,
                                        tile.tilePos.x, tile.tilePos.y, tile.tilePos.z);
    std::ofstream file(fileName.c_str(), std::ios::binary | std::ios::app | std::ios::ate);
    if (file.tellp() > 0)
    {
        m_logger.debug("Reopening file %s to flush %d points",
                        fileName, tile.numPoints());
    }
    for (size_t i = 0; i < tile.numPoints(); ++i)
    {
        file.write((const char*)&tile.position[3*i], 3*sizeof(float));
        file.write((const char*)&tile.intensity[i], sizeof(float));
    }
    tile.position.clear();
    tile.position.shrink_to_fit();
    tile.intensity.clear();
    tile.intensity.shrink_to_fit();
}


//------------------------------------------------------------------------------
inline void fixLasFileName(std::string& fileName)
{
#   ifdef _WIN32
    // Hack: liblas doesn't like forward slashes as path separators on windows
    std::replace(fileName.begin(), fileName.end(), '/', '\\');
#   endif
}


void convertLasToPointDb(const std::string& outDirName,
                         const std::vector<std::string>& lasFileNames,
                         const Imath::Box3d& boundingBox, double tileSize,
                         Logger& logger)
{
    PointDbWriter dbWriter(outDirName, boundingBox, tileSize, 1000000, logger);
    bool useBounds = !boundingBox.isEmpty();
    for (size_t fileIdx = 0; fileIdx < lasFileNames.size(); ++fileIdx)
    {
        std::string fileName = lasFileNames[fileIdx];
        fixLasFileName(fileName);
        LASreadOpener lasReadOpener;
        lasReadOpener.set_file_name(fileName.c_str());
        std::unique_ptr<LASreader> lasReader(lasReadOpener.open());
        if(!lasReader)
            throw DisplazError("Could not open file: %s", fileName);
        uint64_t totPoints = std::max<uint64_t>(lasReader->header.extended_number_of_point_records,
                                                lasReader->header.number_of_point_records);
        logger.info("File %s: %d points", fileName, totPoints);
        logger.progress("Ingest file %d", fileIdx);
        uint64_t pointsRead = 0;
        while (lasReader->read_point())
        {
            const LASpoint& point = lasReader->point;
            V3d P = V3d(point.get_x(), point.get_y(), point.get_z());
            pointsRead += 1;
            logger.progress(double(pointsRead)/totPoints);
            if (pointsRead % 1000000 == 0)
                logger.debug("Cache size: %.2fMB", dbWriter.cacheSizeBytes()/1000000.0);
            if (useBounds && !boundingBox.intersects(P))
                continue;
            dbWriter.writePoint(P, point.intensity);
        }
    }
    dbWriter.close();
}

