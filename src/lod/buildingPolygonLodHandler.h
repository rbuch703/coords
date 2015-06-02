
#ifndef BUILDING_POLYGON_LOD_HANDLER_H
#define BUILDING_POLYGON_LOD_HANDLER_H

#include <string>
#include "polygonLodHandler.h"

class BuildingPolygonLodHandler: public PolygonLodHandler
{

public:
    BuildingPolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual bool applicable(TagDictionary &tags, bool isClosedRing) const;
    
};
   
#endif
