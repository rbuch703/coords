
#ifndef ROAD_MINOR_LABEL_LOD_HANDLER_H
#define ROAD_MINOR_LABEL_LOD_HANDLER_H

#include "lodHandler.h"

class RoadMinorLabelLodHandler: public LodHandler {

public:
    RoadMinorLabelLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing, double area) const;
    virtual int8_t getZIndex(const TagDictionary &tags) const;
    virtual bool isArea() const;
};

#endif
