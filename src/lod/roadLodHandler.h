
#include "lodHandler.h"

class RoadLodHandler: public LodHandler {

public:
    RoadLodHandler(std::string tileDirectory, std::string baseName);

    virtual int applicableUpToZoomLevel(TagDictionary &tags, bool isClosedRing) const;
    virtual bool isArea() const;
};
