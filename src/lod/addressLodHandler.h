
#ifndef ADDRESS_LOD_HANDLER_H
#define ADDRESS_LOD_HANDLER_H

#include "lodHandler.h"

class AddressLodHandler: public LodHandler {

public:
    AddressLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double area) const;
    virtual bool isArea() const;
};

#endif
