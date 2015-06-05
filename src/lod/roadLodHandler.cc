
#include "roadLodHandler.h"
#include <iostream>

RoadLodHandler::RoadLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods({10, 9, 6});
}


int RoadLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/) const
{
    if (tags.count("highway"))
    {
        const std::string &type = tags.at("highway");
        if (type == "motorway" || type == "motorway_link" || 
            type == "trunk"    || type == "trunk_link")
            return 0;
        
        if (type == "primary" || type == "primary_link")
            return 7;
            
        if (type == "secondary" || type == "secondary_link")
            return 9;
            
        if (type == "tertiary" || type == "tertiary_link")
            return 10;
            
        if (type == "residential" || type == "unclassified" || type == "road")
            return 10;
        
        if (type == "living_street" || type == "service" || type == "pedestrian" ||
            type == "raceway")
            return 12;
            
        if (type == "platform" || type == "steps" || type == "bridleway" || 
            type == "footway"  || type == "path"  || type == "track"     ||
            type == "cycleway")
            return 13;

        //these types are known and judged as irrelevant for rendering;
        //all other types are unknown        
        if (type != "construction" && type != "proposed" && type != "no" &&
            type != "byway" && type != "unsurfaced")
            std::cout << "unknown road type '" << type << "'." << std::endl;
    }
    
    if (tags.count("railway"))
    {
        const std::string &type = tags.at("railway");

        if (type == "rail") return 0;
        
        if (type == "abandoned"    || type == "tram"         || type == "disused"    || 
            type == "subway"       || type == "narrow_gauge" || type == "light_rail" || 
            type == "preserved"    || type == "monorail"     || type == "funicular" )
            return 10;
    }
    

    return -1;
}

bool RoadLodHandler::isArea() const
{
    return false;
}

