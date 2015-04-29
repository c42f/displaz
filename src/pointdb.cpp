// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "pointdb.h"

#include <fstream>

#include "logger.h"


//------------------------------------------------------------------------------
struct SimplePointDb::PointDbTile
{
    PointDbTile(TilePos tilePos, const std::string& fileName)
        : tilePos(tilePos), fileName(fileName), recentlyUsed(false) {}

    TilePos tilePos;
    std::string fileName;
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

    void clear()
    {
        position.clear();
        position.shrink_to_fit();
        intensity.clear();
        intensity.shrink_to_fit();
    }
};


//------------------------------------------------------------------------------
SimplePointDb::SimplePointDb(const std::string& dirName, size_t cacheMaxSize, Logger& logger)
    : m_dirName(dirName),
    m_boundingBox(),
    m_tileSize(0),
    m_offset(0),
    m_maxCacheSize(cacheMaxSize),
    m_cacheByteSize(0),
    m_bytesSinceTrim(0),
    m_logger(logger)
{
    m_logger.debug("Using SimplePointDb cache size: %.2f MB", cacheMaxSize/(1024.0*1024.0));
    readConfig();
}


// Declared here to keep definition of PointDbTile out of header
SimplePointDb::~SimplePointDb() {}


void SimplePointDb::query(const Imath::Box3d& boundingBox,
                          std::vector<float>& position,
                          std::vector<float>& intensity)
{
    position.clear();
    intensity.clear();
    int startx = (int)floor(boundingBox.min.x/m_tileSize);
    int starty = (int)floor(boundingBox.min.y/m_tileSize);
    int startz = (int)floor(boundingBox.min.z/m_tileSize);
    int endx =   (int)ceil(boundingBox.max.x/m_tileSize);
    int endy =   (int)ceil(boundingBox.max.y/m_tileSize);
    int endz =   (int)ceil(boundingBox.max.z/m_tileSize);
    Imath::Box3f offsetBox(boundingBox.min - m_offset,
                           boundingBox.max - m_offset);
    for (int tileZ = startz; tileZ < endz; ++tileZ)
    for (int tileY = starty; tileY < endy; ++tileY)
    for (int tileX = startx; tileX < endx; ++tileX)
    {
        const PointDbTile* tile = findTile(TilePos(tileX,tileY,tileZ));
        if (!tile)
            continue;
        size_t numPoints = tile->numPoints();
        for (size_t i = 0; i < numPoints; ++i)
        {
            float x = tile->position[3*i];
            float y = tile->position[3*i+1];
            float z = tile->position[3*i+2];
            if (x < offsetBox.min.x || x >= offsetBox.max.x ||
                y < offsetBox.min.y || y >= offsetBox.max.y ||
                z < offsetBox.min.z || z >= offsetBox.max.z)
            {
                continue;
            }
            position.push_back(x);
            position.push_back(y);
            position.push_back(z);
            intensity.push_back(tile->intensity[i]);
        }
    }
}


const SimplePointDb::PointDbTile* SimplePointDb::findTile(const TilePos& pos)
{
    auto it = m_cache.find(pos);
    if (it == m_cache.end())
        return 0;
    PointDbTile& tile = *it->second;
    tile.recentlyUsed = true;
    if (tile.empty())
    {
        readTileFromDisk(tile);
        size_t s = tile.sizeBytes();
        m_bytesSinceTrim += s;
        m_cacheByteSize += s;
        if (m_cacheByteSize > m_maxCacheSize)
            trimCache(true);
        else if (m_bytesSinceTrim > m_maxCacheSize/2)
        {
            m_bytesSinceTrim = 0;
            trimCache(false);
        }
    }
    assert(!tile.empty());
    return &tile;
}


void SimplePointDb::trimCache(bool doClear)
{
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
    {
        PointDbTile& tile = *it->second;
        if (tile.recentlyUsed)
            tile.recentlyUsed = false;
        else if (doClear)
            tile.clear();
    }
}


void SimplePointDb::readConfig()
{
    std::string configFileName = tfm::format("%s/config.txt", m_dirName);
    std::ifstream dbConfig(configFileName.c_str());
    dbConfig >> m_tileSize
                >> m_boundingBox.min.x >> m_boundingBox.min.y >> m_boundingBox.min.z
                >> m_boundingBox.max.x >> m_boundingBox.max.y >> m_boundingBox.max.z
                >> m_offset.x >> m_offset.y >> m_offset.z;
    if (!dbConfig)
        throw DisplazError("Could not read DB config file: %s", configFileName);
    while (true)
    {
        TilePos pos;
        dbConfig >> pos.x >> pos.y >> pos.z;
        if (!dbConfig)
            break;
        std::string fileName = tfm::format("%s/%d_%d_%d.dat", m_dirName, pos.x, pos.y, pos.z);
        m_cache.insert(std::make_pair(pos,
                std::unique_ptr<PointDbTile>(new PointDbTile(pos, fileName))));
    }
    m_logger.info("Loaded config file: %s; %d tiles", configFileName, m_cache.size());
}


void SimplePointDb::readTileFromDisk(PointDbTile& tile)
{
    std::ifstream file(tile.fileName, std::ios::binary | std::ios::ate);
    size_t numPoints = file.tellg()/(4*sizeof(float));
    tile.position.resize(3*numPoints);
    tile.intensity.resize(numPoints);
    file.seekg(0);
    for (size_t i = 0; i < numPoints; ++i)
    {
        file.read((char*)&tile.position[3*i], 3*sizeof(float));
        file.read((char*)&tile.intensity[i], sizeof(float));
    }
    if (!file)
        throw DisplazError("Error reading points for tile at %d", tile.tilePos);
    m_logger.debug("Cache tile: %d", tile.tilePos);
}

