
#include <iostream>

#include "ringSegment.h"


RingSegment::RingSegment(const OsmGeoPosition &start, const OsmGeoPosition &end, int64_t wayId):
        wayId(wayId), start(start), end(end), child1(nullptr), child2(nullptr), 
        isSegmentReversed(false)
{
}

RingSegment::RingSegment( const OsmWay &way ):
    wayId(way.id), child1(nullptr), child2(nullptr), isSegmentReversed(false)
{
    MUST(way.refs.size() > 0, "trying to create ring segment from empty way");
    start = way.refs.front();
    end   = way.refs.back();
}

RingSegment::RingSegment( RingSegment *pChild1, RingSegment *pChild2):
    wayId(-1), child1(pChild1), child2(pChild2), isSegmentReversed(false)
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
OsmGeoPosition RingSegment::getStartPosition() const { return isSegmentReversed ? end    : start; }
OsmGeoPosition RingSegment::getEndPosition()   const { return isSegmentReversed ? start  : end;   }
RingSegment*   RingSegment::getFirstChild()    const { return isSegmentReversed ? child2 : child1;}
RingSegment*   RingSegment::getSecondChild()   const { return isSegmentReversed ? child1 : child2;}

bool RingSegment::isReversed() const { return this->isSegmentReversed;}


int64_t RingSegment::getWayId() const { return wayId; }


void RingSegment::reverse() { isSegmentReversed = !isSegmentReversed; }


std::vector<uint64_t> RingSegment::getWayIdsRecursive() const
{
    std::vector<uint64_t> wayIds;
    
    this->getWayIdsRecursive(wayIds);    
    return wayIds;

}

void RingSegment::getWayIdsRecursive(std::vector<uint64_t> &wayIdsOut) const
{
    if ( this->wayId != -1) //if not an internal tree node
        wayIdsOut.push_back(this->wayId);

    if (this->child1) this->child1->getWayIdsRecursive(wayIdsOut);
    if (this->child2) this->child2->getWayIdsRecursive(wayIdsOut);
}


void RingSegment::printHierarchy( int depth) const
{
    std::string depthSpaces = "";
    for (int i = 0; i < depth; i++)
        depthSpaces += "  ";
        
    std::cout << depthSpaces << " (way " <<  this->wayId << ") [" 
              << this->start.id << "; " << this->end.id << "]" << std::endl;
    if (this->child1)
        this->child1->printHierarchy(depth + 1);
        
    if (this->child2)
        this->child2->printHierarchy(depth + 1);
}


