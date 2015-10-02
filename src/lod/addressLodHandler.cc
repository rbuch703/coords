
#include "addressLodHandler.h"
#include "config.h"

AddressLodHandler::AddressLodHandler(std::string tileDirectory, std::string baseName): LodHandler(tileDirectory, baseName)
{
//    enableLods({10, 9, 6});
}


int AddressLodHandler::applicableUpToZoomLevel(TagDictionary &tags, bool/* isClosedRing*/, double) const
{
    if (tags.count("addr:housenumber"))
        return 10;

    return -1;
}

bool AddressLodHandler::isArea() const
{
    return false;
}

