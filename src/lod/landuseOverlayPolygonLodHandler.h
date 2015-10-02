
#ifndef LANDUSE_OVERLAY_POLYGON_LOD_HANDLER_H
#define LANDUSE_OVERLAY_POLYGON_LOD_HANDLER_H

#include <string>
#include <set>
#include "lodHandler.h"

class LanduseOverlayPolygonLodHandler: public LodHandler
{

public:
    LanduseOverlayPolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double area) const;
    virtual bool isArea() const;

public:
    
};
   
#endif
