
#ifndef WATER_POLYGON_LOD_HANDLER_H
#define WATER_POLYGON_LOD_HANDLER_H

#include <string>
#include <set>
#include "polygonLodHandler.h"

class WaterPolygonLodHandler: public PolygonLodHandler
{

public:
    WaterPolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual bool applicable(TagDictionary &tags, bool isClosedRing) const;

public:
     //static std::set<std::string> landuses, leisures, naturals, amenities;
     
};
   
#endif
