
#include "boundaryLodHandler.h"
#include "config.h"

BoundaryLodHandler::BoundaryLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
    enableLods( {11} );
}


int BoundaryLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/, double) const
{
    if (tags.count("boundary") && tags.at("boundary") == "administrative")
    {
        if (tags.count("admin_level"))
        {
            if (tags.at("admin_level") == "1" || tags.at("admin_level") == "2")
                return 0;

            if (tags.at("admin_level") == "3" || tags.at("admin_level") == "4")
                return 0;

            if (tags.at("admin_level") == "5" || tags.at("admin_level") == "6")
                return 0;
                
        }
        return -1;
    }
    
    return -1;
}

bool BoundaryLodHandler::isArea() const
{
    return false;
}

