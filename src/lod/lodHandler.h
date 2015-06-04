
#ifndef LOD_HANDLER_H
#define LOD_HANDLER_H

#include <vector>
#include <string>

#include "misc/rawTags.h"
#include "geom/genericGeometry.h"
#include "tiles.h"

class LodHandler
{

public:
    LodHandler(std::string tileDirectory, std::string baseName);
    
    // non-copyable for now
    LodHandler( LodHandler &other);
    LodHandler& operator=(LodHandler &other);
    
    virtual ~LodHandler();
    const void* const* getZoomLevels() const;
    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const = 0;
    virtual bool isArea() const = 0;
    
    void cleanupFiles() const;

    void store (const GenericGeometry &geometry, const Envelope &env, int zoomLevel);
    void store (const GenericGeometry &geometry, const Envelope &env);
    void closeFiles();
    void subdivide();

public:     
    static const int MAX_ZOOM_LEVEL = 24;
    static const Envelope mercatorWorldBounds;
    static const uint64_t MAX_META_NODE_SIZE = 500ll * 1000 * 1000;
    static const uint64_t MAX_NODE_SIZE      =   5ll * 1000 * 1000;

protected:

    std::string tileDirectory;
    std::string baseName;
    FileBackedTile* lodTileSets[MAX_ZOOM_LEVEL+1];
    //std::vector<FileBackedTile*> lodTileSets;
    FileBackedTile baseTileSet;
};

#endif
