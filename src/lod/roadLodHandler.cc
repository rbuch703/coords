
#include "roadLodHandler.h"
#include "config.h"

#include <map>

RoadLodHandler::RoadLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods({10, 9, 6});
}

//taken from IMPOSM 3 source
static const std::map<std::string, int64_t> highwayTypeZBias = { 
    {"minor",          3},
    {"road" ,          3},
    {"unclassified",   3},
	{"residential",    3},
	{"tertiary_link",  3},
	{"tertiary",       4},
	{"secondary_link", 3},
	{"secondary",      5},
	{"primary_link",   3},
	{"primary",        6},
	{"trunk_link",     3},
	{"trunk",          8},
	{"motorway_link",  3},
	{"motorway",       9},
};

int8_t RoadLodHandler::getZIndex(const TagDictionary &tags) const
{
    int64_t zIndex = 0;
    if (tags.count("layer"))
    {
        zIndex += 10 * atoi( tags.at("layer").c_str());
    }
    
    if (tags.count("highway"))
    {
        std::string type = tags.at("highway");
        if ( highwayTypeZBias.count(type))
            zIndex += highwayTypeZBias.at(type);
    } else if (tags.count("railway"))
    {
        zIndex += 7;
    }
    
    if (tags.count("tunnel"))
    {
        std::string val = tags.at("tunnel");
        if (val == "true" || val == "yes" || val == "1")
            zIndex -= 10;
    }

    if (tags.count("bridge"))
    {
        std::string val = tags.at("bridge");
        if (val == "true" || val == "yes" || val == "1")
            zIndex += 10;
    }

    return zIndex < -128 ? -128 : (zIndex > 127 ? 127 : zIndex);
}


int RoadLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/) const
{
    if (tags.count("highway"))
    {
        const std::string &type = tags.at("highway");
        tags.insert(std::make_pair("type", type));

        if (type == "motorway" || type == "trunk")
        {
            tags.insert(std::make_pair("stylegroup", "motorway"));
            return 0;
        }
        
        if (type == "primary" || type == "secondary")
        {
            tags.insert(std::make_pair("stylegroup", "mainroad"));
            return 7;
        }
            
        if ( type == "motorway_link" || type == "trunk_link" || type == "primary_link" ||
             type == "secondary_link" || type == "tertiary" || type == "tertiary_link" ||
             type == "residential" || type == "unclassified" || type == "road" ||
             type == "living_street")
        {
            tags.insert(std::make_pair("stylegroup", "minorroad"));
            
            return 10;
        }
            
        
        if ( type == "service" || type == "track")
        { 
            tags.insert(std::make_pair("stylegroup", "service"));
            return 12;
        }   
            
            /*type == "raceway")
        if (type == "platform" ||  || */
       if (type == "path"  || type == "cycleway" || type == "footway"  ||
           type == "pedestrian" || type == "steps" || type == "bridleway")
       {
            tags.insert(std::make_pair("noauto", "service"));
            
            return 13;
       }


        //these types are known, and judged as irrelevant for rendering;
        //all other types are unknown        
        /*if (type != "construction" && type != "proposed" && type != "no" &&
            type != "byway" && type != "unsurfaced")
            std::cout << "unknown road type '" << type << "'." << std::endl;*/
    }
    
    if (tags.count("railway"))
    {
        const std::string &type = tags.at("railway");
        tags.insert(std::make_pair("type", type));
        tags.insert(std::make_pair("stylegroup", "railway"));

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
