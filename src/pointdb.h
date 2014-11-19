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

#ifndef DISPLAZ_POINTDB_H_INCLUDED
#define DISPLAZ_POINTDB_H_INCLUDED

#include <map>
#include <memory>
#include <vector>

#include "util.h"

class Logger;

/// Reader for simple point database format
///
/// The idea here is to be able to fairly quickly query for all points within a
/// bounding box, when the full set of points is larger than available memory.
/// For typical airborne laser scanning parameters the point density is
/// reasonably predictable so we just grid up the domain into fixed size tiles.
class SimplePointDb
{
    public:
        SimplePointDb(const std::string& dirName, size_t cacheMaxSize, Logger& logger);

        ~SimplePointDb();

        /// Return all points within the given bounding box
        ///
        /// Point positions are relative to the overall offset
        void query(const Imath::Box3d& boundingBox,
                   std::vector<float>& position,
                   std::vector<float>& intensity);

        /// Return offset of coordinate system from origin
        Imath::V3d offset() const { return m_offset; }

    private:
        struct PointDbTile;

        const PointDbTile* findTile(const TilePos& pos);

        void trimCache(bool doClear);

        void readConfig();

        void readTileFromDisk(PointDbTile& tile);

        std::string m_dirName;
        Imath::Box3d m_boundingBox;
        double m_tileSize;
        Imath::V3d m_offset;
        std::map<TilePos, std::unique_ptr<PointDbTile>> m_cache;
        size_t m_maxCacheSize;
        size_t m_cacheByteSize;
        size_t m_bytesSinceTrim;
        Logger& m_logger;
};



//------------------------------------------------------------------------------
//class PointDb
//{
//    public:
////        virtual void initialize(const std::vector<GeomField>& requestFields) = 0;
//
//        virtual size_t query(const Imath::Box3d& boundingBox,
//                             std::vector<GeomField>& fields, size_t npoints) = 0;
//};



//        virtual void initialize(const std::vector<GeomField>& requestFields)
//        {
//            for (size_t i = 0; i < requestFields.size(); ++i)
//            {
//                const GeomField& f = requestFields[i];
//                if (!((f.name == "position"  && f.spec == TypeSpec::vec3float32()) ||
//                      (f.name == "intensity" && f.spec == TypeSpec::uint16_i()) ||
//                      (f.name == "color"     && f.spec == TypeSpec(TypeSpec::Uint,2,3,TypeSpec::Color))
//                    ))
//                {
//                    throw DisplazError("Unrecognized field: %s", f);
//                }
//            }
//            m_fields = requestFields;
//
//        }


#endif // DISPLAZ_POINTDB_H_INCLUDED
