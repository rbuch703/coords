
#include "waterPolygonLodHandler.h"

/*std::set<std::string> LandusePolygonLodHandler::naturals =
{
    "wood", "land", "island", "scrub", "wetland"
};*/


WaterPolygonLodHandler::WaterPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods({13, 9, 7});
}


int WaterPolygonLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const
{
    if (!isClosedRing)
        return -1;

    if (tags.count("waterway") && tags["waterway"] == "riverbank") return 0;
    if (tags.count("natural")  && tags["natural"]  == "water")     return 0;
    
    if (tags.count("landuse") && (tags["landuse"] == "basin" || tags["landuse"] == "reservoir"))
        return 0;
        
    return -1;
    
}

bool WaterPolygonLodHandler::isArea() const
{
    return true;
}

