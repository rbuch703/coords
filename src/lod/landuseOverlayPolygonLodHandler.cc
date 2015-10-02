
#include "landuseOverlayPolygonLodHandler.h"
#include "config.h"


LanduseOverlayPolygonLodHandler::LanduseOverlayPolygonLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    //FIXME: not validated
    enableLods({12, 9, 7, 5});
}


int LanduseOverlayPolygonLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double) const
{
    if (!isClosedRing)
        return -1;
        
    if (tags.count("leisure") && ( tags["leisure"] == "nature_reserve" || tags["leisure"] == "wetland")) 
    {
        tags.insert( make_pair("type", tags["leisure"]));
        return 0;
    }
    
    return -1;
    
}

bool LanduseOverlayPolygonLodHandler::isArea() const
{
    return true;
}

