
#ifndef RINGSEGMENT_H
#define RINGSEGMENT_H

#include "osm/osmTypes.h"
#include "osm/osmMappedTypes.h"

class RingSegment
{
public:
    RingSegment( const OsmGeoPosition &start, const OsmGeoPosition &end);    
    RingSegment( const OsmLightweightWay &way);
    RingSegment( RingSegment *pChild1, RingSegment *pChild2);
    bool isClosed() const;
    OsmGeoPosition getStartPosition() const;
    OsmGeoPosition getEndPosition()   const;
    RingSegment*   getFirstChild()    const;
    RingSegment*   getSecondChild()   const;
    int64_t getWayId() const;
private:
    void reverse();

public:
    /* id of the OSM way this ring segment is based on (in which case the ring segment must
       not have children); or -1 if the ring segment is not directly based on a single OSM way
       and does have children */
    int64_t wayId; 
    OsmGeoPosition start;
    OsmGeoPosition end;
    
    /* NOTE: RingSegment does *not* take ownership of the child RingSegments it points to.
     *       Those have to be delete'ed separately */
    RingSegment *child1, *child2; 
    bool isReversed;       // reverse node order before connecting to its sibling
};

#endif

