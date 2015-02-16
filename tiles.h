#ifndef TILES_H
#define TILES_H

#include <iostream>
#include <list>
#include <string>
#include "osmMappedTypes.h"

class GeoAABB{
public:
    GeoAABB() {};
    GeoAABB( int32_t lat, int32_t lng);
    GeoAABB( int32_t latMin, int32_t latMax, int32_t lngMin, int32_t lngMax);

    void add (int32_t lat, int32_t lng);
    bool overlapsWith(const GeoAABB &other) const;
    static const GeoAABB& getWorldBounds();
    
public:
    int32_t latMin, latMax;
    int32_t lngMin, lngMax;
    
};

GeoAABB getBounds(const OsmLightweightWay &way);
std::ostream& operator<<(std::ostream &os, const GeoAABB &aabb);

class MemoryBackedTile {
public:
    MemoryBackedTile(const char*fileName, const GeoAABB &bounds, uint64_t maxNodeSize);
    ~MemoryBackedTile();
    
    void add(const OsmLightweightWay &way, const GeoAABB &wayBounds);
    void writeToDiskRecursive(bool includeSelf);
    
    uint64_t getSize() const { return size; }
public:
    MemoryBackedTile *topLeftChild, *topRightChild, *bottomLeftChild, *bottomRightChild;


private:
    void subdivide();

private:
    std::list<OsmLightweightWay> ways;
    GeoAABB bounds;
    std::string fileName;
    uint64_t size;
    uint64_t maxNodeSize;
};

class FileBackedTile {
public:
    FileBackedTile(const char*fileName, const GeoAABB &bounds, uint64_t maxNodeSize);
    ~FileBackedTile();
    void add(OsmLightweightWay &way, const GeoAABB &wayBounds);
    void releaseMemoryResources();
    void subdivide(uint64_t maxSubdivisionNodeSize, bool useMemoryBackedStorage = false);
private:
    void subdivide();

private:
    FILE* fData;
    GeoAABB bounds;
    std::string fileName;
    uint64_t size;
    uint64_t maxNodeSize;
    FileBackedTile *topLeftChild, *topRightChild, *bottomLeftChild, *bottomRightChild;
};


#endif

