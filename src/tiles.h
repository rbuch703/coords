#ifndef TILES_H
#define TILES_H

#include <iostream>
#include <list>
#include <string>
#include "osm/osmMappedTypes.h"
#include "geom/envelope.h"


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

