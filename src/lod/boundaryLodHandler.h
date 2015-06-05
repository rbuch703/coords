
#ifndef BOUNDARY_LOD_HANDLER_H
#define BOUNDARY_LOD_HANDLER_H

#include "lodHandler.h"

class BoundaryLodHandler: public LodHandler {

public:
    BoundaryLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const;
    virtual bool isArea() const;
};

#endif
