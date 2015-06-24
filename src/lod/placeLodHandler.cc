
#include "placeLodHandler.h"
#include "config.h"

PlaceLodHandler::PlaceLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
//    enableLods({10, 9, 6});
}


int PlaceLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/) const
{
    if (tags.count("place"))
        return 0;

    return -1;
}

bool PlaceLodHandler::isArea() const
{
    return false;
}

