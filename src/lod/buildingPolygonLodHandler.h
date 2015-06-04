
#ifndef BUILDING_POLYGON_LOD_HANDLER_H
#define BUILDING_POLYGON_LOD_HANDLER_H

#include <string>
#include "lodHandler.h"

class BuildingPolygonLodHandler: public LodHandler
{

public:
    BuildingPolygonLodHandler(std::string tileDirectory, std::string baseName);
    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const;
    virtual bool isArea() const;
    
};
   
#endif
