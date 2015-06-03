
#ifndef POLYGON_LOD_HANDLER_H
#define POLYGON_LOD_HANDLER_H

#include <vector>
#include <string>

#include "misc/rawTags.h"
#include "geom/genericGeometry.h"
#include "tiles.h"

class PolygonLodHandler
{

public:
    PolygonLodHandler(std::string tileDirectory, std::string baseName);
    
    // non-copyable for now
    PolygonLodHandler( PolygonLodHandler &other);
    PolygonLodHandler& operator=(PolygonLodHandler &other);
    
    virtual ~PolygonLodHandler();
    void cleanupFiles() const;

    const void* const* getZoomLevels() const;
    virtual bool applicable(TagDictionary &tags, bool isClosedRing) const = 0;
    void store (const GenericGeometry &geometry, const Envelope &env, int zoomLevel);
    void store (const GenericGeometry &geometry, const Envelope &env);
    void closeFiles();
    void subdivide();

public:     
    static const int MAX_ZOOM_LEVEL = 24;
    static const Envelope mercatorWorldBounds;
    static const uint64_t MAX_META_NODE_SIZE = 500ll * 1000 * 1000;
    static const uint64_t MAX_NODE_SIZE      =  10ll * 1000 * 1000;

protected:

    std::string tileDirectory;
    std::string baseName;
    FileBackedTile* lodTileSets[MAX_ZOOM_LEVEL+1];
    //std::vector<FileBackedTile*> lodTileSets;
    FileBackedTile baseTileSet;
};
   
#endif
