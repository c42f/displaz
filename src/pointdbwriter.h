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

#ifndef DISPLAZ_POINTDBWRITER_H_INCLUDED
#define DISPLAZ_POINTDBWRITER_H_INCLUDED

#include <map>
#include <stdint.h>
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
