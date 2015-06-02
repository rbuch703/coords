
#ifndef LANDUSE_POLYGON_LOD_HANDLER_H
#define LANDUSE_POLYGON_LOD_HANDLER_H

#include <string>
#include <set>
#include "polygonLodHandler.h"

class LandusePolygonLodHandler: public PolygonLodHandler
{

public:
    LandusePolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual bool applicable(TagDictionary &tags, bool isClosedRing) const;

public:
     static std::set<std::string> landuses, leisures, naturals, amenities;
     
};
   
#endif
