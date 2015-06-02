
#include "waterPolygonLodHandler.h"

/*std::set<std::string> LandusePolygonLodHandler::naturals =
{
    "wood", "land", "island", "scrub", "wetland"
};*/


WaterPolygonLodHandler::WaterPolygonLodHandler(std::string tileDirectory, std::string baseName): PolygonLodHandler(tileDirectory, baseName)
{
    for (int i : {13, 9, 7})
    {
        //std::cout << "#" << i << std::endl;
        char num[4];
        MUST( snprintf(num, 4, "%d", i) < 4, "overflow");
        lodTileSets[i] = new FileBackedTile(tileDirectory + baseName + "_" + num + "_", mercatorWorldBounds, MAX_META_NODE_SIZE);
    }
}


bool WaterPolygonLodHandler::applicable(TagDictionary &tags, bool isClosedRing) const
{
    if (!isClosedRing)
        return false;

    if (tags.count("waterway") && tags["waterway"] == "riverbank") return true;
    if (tags.count("natural")  && tags["natural"]  == "water")     return true;
    
    if (tags.count("landuse") && (tags["landuse"] == "basin" || tags["landuse"] == "reservoir"))
        return true;
        
    return false;
    
}

