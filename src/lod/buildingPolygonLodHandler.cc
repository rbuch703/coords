
#include "buildingPolygonLodHandler.h"


BuildingPolygonLodHandler::BuildingPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    for (int i : {10, 11, 12, 13, 14})
    {
        char num[4];
        MUST( snprintf(num, 4, "%d", i) < 4, "overflow");
        lodTileSets[i] = new FileBackedTile(tileDirectory + baseName + "_" + num + "_", mercatorWorldBounds, MAX_META_NODE_SIZE);  
    }
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

