
#ifndef WATER_POLYGON_LOD_HANDLER_H
#define WATER_POLYGON_LOD_HANDLER_H

#include <string>
#include <set>
#include "lodHandler.h"

class WaterPolygonLodHandler: public LodHandler
{

public:
    WaterPolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double area) const;
    virtual bool isArea() const;

public:
     //static std::set<std::string> landuses, leisures, naturals, amenities;
     
};
   
#endif
