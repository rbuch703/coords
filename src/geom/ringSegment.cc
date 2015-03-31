
#include "ringSegment.h"

RingSegment::RingSegment(const OsmGeoPosition &start, const OsmGeoPosition &end):
        start(start), end(end), child1(nullptr), child2(nullptr), 
        isReversed(false)//, removeEnd1(false)
{
}

RingSegment::RingSegment( const OsmLightweightWay &way ):
    wayId(way.id), child1(nullptr), child2(nullptr), isReversed(false)
{
    MUST(way.numVertices > 0, "cannot create ring from way without members");
    start = way.vertices[0];
    end   = way.vertices[way.numVertices - 1];
}

RingSegment::RingSegment( RingSegment *pChild1, RingSegment *pChild2):
    wayId(-1), child1(pChild1), child2(pChild2), isReversed(false) 
{
    
        if (child2->getEndPosition() == child1->getStartPosition() ||
            child2->getEndPosition() == child1->getEndPosition())
            child2->reverse();
        
        /* now (the) one of the end points of child2 that can be connected to
         * child1 is the 'start' position of child2 */
        
        if (child1->getStartPosition() == child2->getStartPosition())
            child1->reverse();
            
        MUST( child1->getEndPosition() == child2->getStartPosition(), "invalid geometry processing");
        
                   
    start = child1->getStartPosition();
    end   = child2->getEndPosition();
}


bool RingSegment::isClosed() const { return (start == end); }
OsmGeoPosition RingSegment::getStartPosition() const { return isReversed ? end    : start; }
OsmGeoPosition RingSegment::getEndPosition()   const { return isReversed ? start  : end;   }
RingSegment*   RingSegment::getFirstChild()    const { return isReversed ? child2 : child1;}
RingSegment*   RingSegment::getSecondChild()   const { return isReversed ? child1 : child2;}

int64_t RingSegment::getWayId() const { return wayId; }


void RingSegment::reverse() { isReversed = !isReversed; }

