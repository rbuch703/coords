#ifndef TILES_H
#define TILES_H

#include <iostream>
#include <list>
#include <string>
#include "osm/osmMappedTypes.h"
#include "geom/envelope.h"


class MemoryBackedTile {
public:
    MemoryBackedTile(const char*fileName, const Envelope &bounds, uint64_t maxNodeSize);
    ~MemoryBackedTile();
    
    void add(const OsmLightweightWay &way, const Envelope &wayBounds);
    void writeToDiskRecursive(bool includeSelf);
    
    uint64_t getSize() const { return size; }
public:
    MemoryBackedTile *topLeftChild, *topRightChild, *bottomLeftChild, *bottomRightChild;


private:
    void subdivide();

private:
    std::list<OsmLightweightWay> ways;
    Envelope bounds;
    std::string fileName;
    uint64_t size;
    uint64_t maxNodeSize;
};

class FileBackedTile {
public:
    FileBackedTile(const char*fileName, const Envelope &bounds, uint64_t maxNodeSize);
    ~FileBackedTile();
    void add(OsmLightweightWay &way, const Envelope &wayBounds);
    void releaseMemoryResources();
    void subdivide(uint64_t maxSubdivisionNodeSize, bool useMemoryBackedStorage = false);
private:
    void subdivide();

private:
    FILE* fData;
    Envelope bounds;
    std::string fileName;
    uint64_t size;
    uint64_t maxNodeSize;
    FileBackedTile *topLeftChild, *topRightChild, *bottomLeftChild, *bottomRightChild;
};


#endif

