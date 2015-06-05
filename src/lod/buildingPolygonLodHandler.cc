
#include "buildingPolygonLodHandler.h"


BuildingPolygonLodHandler::BuildingPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods({10, 11, 12, 13, 14});
}


int BuildingPolygonLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const
{
    if (!isClosedRing)
        return -1;
        
    if (tags.count("building") && tags["building"] != "no")
        return 0;
        
    if (tags.count("railway") && tags["railway"] == "station")
        return 0;
        
    if (tags.count("aeroway") && tags["aeroway"] == "terminal")
        return 0;
        
    return -1;
    
}

bool BuildingPolygonLodHandler::isArea() const
{
    return true;
}

