
#include "waterPolygonLodHandler.h"
#include "config.h"

/*std::set<std::string> LandusePolygonLodHandler::naturals =
{
    "wood", "land", "island", "scrub", "wetland"
};*/

static std::set<std::string> waterways = {
    "basin", "canal", "mill_pond", "pond", "riverbank", "stream"
};

WaterPolygonLodHandler::WaterPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods({13, 9, 7});
}


int WaterPolygonLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double) const
{
    if (!isClosedRing)
        return -1;


    if (tags.count("natural")  && (tags["natural"]  == "water"|| tags["natural"] == "pond" ))
    {
         tags.insert( make_pair("type", tags["natural"]));
         return 0;
    }
    
    if (tags.count("waterway") && waterways.count(tags["waterway"]))
    {
        tags.insert( make_pair( "type", tags["waterway"]));
        return 0;
    }

    if (tags.count("landuse") && (tags["landuse"] == "basin" || tags["landuse"] == "reservoir"))
    {
        tags.insert( make_pair( "type", tags["landuse"]));
        return 0;
    }
        
    return -1;
    
}

bool WaterPolygonLodHandler::isArea() const
{
    return true;
}

