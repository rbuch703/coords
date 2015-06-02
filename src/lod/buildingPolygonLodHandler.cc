
#include "buildingPolygonLodHandler.h"


BuildingPolygonLodHandler::BuildingPolygonLodHandler(std::string tileDirectory, std::string baseName): PolygonLodHandler(tileDirectory, baseName)
{
    for (int i : {10, 11, 12, 13, 14})
    {
        char num[4];
        MUST( snprintf(num, 4, "%d", i) < 4, "overflow");
        lodTileSets[i] = new FileBackedTile(tileDirectory + baseName + "_" + num + "_", mercatorWorldBounds, MAX_META_NODE_SIZE);  
    }
}


bool BuildingPolygonLodHandler::applicable(TagDictionary &tags, bool isClosedRing) const
{
    if (!isClosedRing)
        return false;
        
    if (tags.count("building"))
        return true;
        
    if (tags.count("railway") && tags["railway"] == "station")
        return true;
        
    if (tags.count("aeroway") && tags["aeroway"] == "terminal")
        return true;
        
    return false;
    
}

