// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_POINTDBWRITER_H_INCLUDED
#define DISPLAZ_POINTDBWRITER_H_INCLUDED

#include <cstdint>
#include <map>
#include <vector>

#include "util.h"

#include "logger.h"

/// Writer for a simple on-disk point database format
///
/// The idea here is to create a very simple database which allows spatial
/// bounding box queries from an unordered set of points.  This is done by
/// tiling them into files, working on the assumption that the full set of
/// points may exceed available memory.
class PointDbWriter
{
    public:
        PointDbWriter(const std::string& dirName, const Imath::Box3d& boundingBox,
                      double tileSize, size_t flushInterval, Logger& logger);

        /// Compute current memory usage in bytes of the internal cache
        size_t cacheSizeBytes() const;

        /// Return total number of points written
        uint64_t pointsWritten() const { return m_pointsWritten; }

        /// Write a single point to the database with given position and
        /// intensity
        void writePoint(Imath::V3d P, float intensity);

        /// Close database, and write config file
        void close();

    private:
        struct PointDbTile;

        PointDbTile& findTile(const TilePos& pos);

        void flushTiles(bool forceFlushAll = false);

        void flushToDisk(PointDbTile& tile);

        std::string m_dirName;
        Imath::Box3d m_boundingBox;
        double m_tileSize;
        Imath::V3d m_offset;
        std::map<TilePos, PointDbTile, TilePosLess> m_cache;
        bool m_computeBounds;
        size_t m_flushInterval;
        bool m_haveOffset;
        PointDbTile* m_prevTile;
        uint64_t m_pointsWritten;
        Logger& m_logger;
};


/// Convert a list of las files to PointDb format
void convertLasToPointDb(const std::string& outDirName,
                         const std::vector<std::string>& lasFileNames,
                         const Imath::Box3d& boundingBox, double tileSize,
                         Logger& logger);


#endif // DISPLAZ_POINTDBWRITER_H_INCLUDED
