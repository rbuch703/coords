
#ifndef WATERWAY_LOD_HANDLER_H
#define WATERWAY_LOD_HANDLER_H

#include "lodHandler.h"

class WaterwayLodHandler: public LodHandler {

public:
    WaterwayLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const;
    virtual bool isArea() const;
};

#endif
