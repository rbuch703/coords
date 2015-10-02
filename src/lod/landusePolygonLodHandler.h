
#ifndef LANDUSE_POLYGON_LOD_HANDLER_H
#define LANDUSE_POLYGON_LOD_HANDLER_H

#include <string>
#include <set>
#include "lodHandler.h"

class LandusePolygonLodHandler: public LodHandler
{

public:
    LandusePolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double area) const;
    virtual bool isArea() const;

public:
     static std::set<std::string> landuses, leisures, naturals, amenities;
     
};
   
#endif
