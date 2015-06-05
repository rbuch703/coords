
#include "waterwayLodHandler.h"
#include <iostream>

WaterwayLodHandler::WaterwayLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    for (int i : {9})
    {
        char num[4];
        MUST( snprintf(num, 4, "%d", i) < 4, "overflow");
        lodTileSets[i] = new FileBackedTile(tileDirectory + baseName + "_" + num + "_", mercatorWorldBounds, MAX_META_NODE_SIZE);  
    }
}


int WaterwayLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/) const
{
    if (tags.count("waterway"))
    {
        /*note: waterway=riverbank is not handled here on purpose: "riverbank" is used
         *      to mark the *area* of a river, while all features selected here represent
         *      bodies of water drawn as individual lines */
        const std::string type = tags.at("waterway");
        if (type == "river") return 0;
        if (type == "canal") return 0;
        if (type == "stream") return 10;

        if (type == "ditch") return 12;
        if (type == "drain") return 12;
    }
    
    if (tags.count("barrier") && tags.at("barrier") == "ditch") return 12;
    
    return -1;
}

bool WaterwayLodHandler::isArea() const
{
    return false;
}

