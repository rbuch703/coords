
#ifndef ROAD_LABEL_LOD_HANDLER_H
#define ROAD_LABEL_LOD_HANDLER_H

#include "lodHandler.h"

class RoadLabelLodHandler: public LodHandler {

public:
    RoadLabelLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double area) const;
    virtual int8_t getZIndex(const TagDictionary &tags) const;
    virtual bool isArea() const;
};

#endif
