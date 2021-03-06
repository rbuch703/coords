
#include "buildingPolygonLodHandler.h"
#include "config.h"


BuildingPolygonLodHandler::BuildingPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    // chosen based on measurements on a whole planet dump
    enableLods({10, 11, 12, 13});
}


int BuildingPolygonLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double) const
{
    if (!isClosedRing)
        return -1;
        
    if (tags.count("building") && tags["building"] != "no"
                               && tags["building"] != "0"
                               && tags["building"] != "false")
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

