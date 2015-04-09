
#ifndef RINGSEGMENT_H
#define RINGSEGMENT_H

#include <vector>

#include "osm/osmTypes.h"
#include "osm/osmMappedTypes.h"

class RingSegment
{
public:
    RingSegment( const OsmGeoPosition &start, const OsmGeoPosition &end, int64_t wayId = -1);
    RingSegment( const OsmLightweightWay &way);
    RingSegment( RingSegment *pChild1, RingSegment *pChild2);
    bool isClosed() const;
    OsmGeoPosition getStartPosition() const;
    OsmGeoPosition getEndPosition()   const;
    RingSegment*   getFirstChild()    const;
    RingSegment*   getSecondChild()   const;
    std::vector<uint64_t> getWayIdsRecursive() const;
    int64_t getWayId() const;
    bool isReversed() const;
    void printHierarchy( int depth = 0) const;
    
private:
    void reverse();

    void getWayIdsRecursive(std::vector<uint64_t> &wayIdsOut) const;

private:
    /* id of the OSM way this ring segment is based on (in which case the ring segment must
       not have children); 
       - or (-1) if the ring segment is not directly based on a single OSM way
       and does have children
       - or (-2) if the ring segment does not represent an actual OSM way, but
         rather is an "artificial" segment that connects two ways that do not
         share a common end point (done are part of the heuristic closing of
         broken multipolygon rings).  */
    int64_t wayId; 
    OsmGeoPosition start;
    OsmGeoPosition end;
    
    /* NOTE: RingSegment does *not* take ownership of the child RingSegments it points to.
     *       Those have to be delete'ed separately */
    RingSegment *child1, *child2; 
    bool isSegmentReversed;       // reverse node order before connecting to its sibling
};

#endif

