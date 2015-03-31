
#ifndef RING_H
#define RING_H

#include <vector>

#include "osm/osmTypes.h"
#include "geom/ringSegment.h"

class Ring {
public:
    Ring(RingSegment *rootSegment, LightweightWayStore &ways);

//private:
public:
    std::vector<OsmGeoPosition> vertices;
    std::vector<uint64_t>       wayIds;
    std::vector<Ring*>          children;

};

#endif

