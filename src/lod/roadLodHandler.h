
#ifndef ROAD_LOD_HANDLER_H
#define ROAD_LOD_HANDLER_H

#include "lodHandler.h"

class RoadLodHandler: public LodHandler {

public:
    RoadLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const;
    virtual int8_t getZIndex(const TagDictionary &tags) const;
    virtual bool isArea() const;
};

#endif
