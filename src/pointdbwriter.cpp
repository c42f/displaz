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

#include "pointdbwriter.h"

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

namespace std {
inline bool operator<(const TilePos& p1, const TilePos& p2)
{
    if (p1.x != p2.x)
        return p1.x < p2.x;
    return p1.y < p2.y;
}
}


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
                             int tileSize, size_t flushInterval, Logger& logger)
    : m_dirName(dirName),
    m_boundingBox(boundingBox),
    m_computeBounds(boundingBox.isEmpty()),
    m_tileSize(tileSize),
    m_flushInterval(flushInterval),
    m_offset(0),
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
    TilePos tilePos(m_tileSize*(int64_t)floor(P.x/m_tileSize),
                    m_tileSize*(int64_t)floor(P.y/m_tileSize));
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
        "%d\n"
        "%.17e %.17e %.17e %.17e %.17e %.17e\n"
        "%.17e %.17e %.17e\n",
        m_tileSize,
        m_boundingBox.min.x, m_boundingBox.min.y, m_boundingBox.min.z,
        m_boundingBox.max.x, m_boundingBox.max.y, m_boundingBox.max.z,
        m_offset.x, m_offset.y, m_offset.z
    );

    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
        tfm::format(dbConfig, "%d %d\n", it->second.tilePos.x, it->second.tilePos.y);
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
    std::string fileName = tfm::format("%s/%d_%d.dat", m_dirName,
                                        tile.tilePos.x, tile.tilePos.y);
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
    fileName.replace('/', '\\');
#   endif
}


void convertLasToPointDb(const std::string& outDirName,
                         const std::vector<std::string>& lasFileNames,
                         const Imath::Box3d& boundingBox, int tileSize,
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
        logger.progress("Ingest file %d", fileIdx);
        uint64_t totPoints = lasReader->header.number_of_point_records;
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

