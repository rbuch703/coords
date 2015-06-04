
#include "waterPolygonLodHandler.h"

/*std::set<std::string> LandusePolygonLodHandler::naturals =
{
    "wood", "land", "island", "scrub", "wetland"
};*/


WaterPolygonLodHandler::WaterPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    for (int i : {13, 9, 7})
    {
        //std::cout << "#" << i << std::endl;
        char num[4];
        MUST( snprintf(num, 4, "%d", i) < 4, "overflow");
        lodTileSets[i] = new FileBackedTile(tileDirectory + baseName + "_" + num + "_", mercatorWorldBounds, MAX_META_NODE_SIZE);
    }
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

