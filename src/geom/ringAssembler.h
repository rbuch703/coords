
#ifndef RING_ASSEMBLER_H
#define RING_ASSEMBLER_H

#include <list>
#include <vector>
#include <map>

#include <iostream>
#include "osm/osmMappedTypes.h"


class RingAssembler {

    
public:

    void addWay( const OsmLightweightWay &way) 
    {
        ringSegments.push_back( RingSegment(way));
        
        RingSegment *segment = &ringSegments.back();
        while (true)
        {
            if (segment->isClosed())
            {
                closedRings.push_back(segment);
                break;
            }
        
            if (openEndPoints.count( segment->getStartPosition() ))
            {
                RingSegment *sibling = openEndPoints[segment->getStartPosition()];
                assert( openEndPoints.count( sibling->getStartPosition()) &&
                        openEndPoints.count( sibling->getEndPosition()) );
                openEndPoints.erase( sibling->getStartPosition() );
                openEndPoints.erase( sibling->getEndPosition() );
                ringSegments.push_back( RingSegment(segment, sibling));
                segment = &ringSegments.back();
                //could still be closed in itself or connect to another open segment
                // --> loop again
                continue; 
            }

            if (openEndPoints.count( segment->getEndPosition() ))
            {
                RingSegment *sibling = openEndPoints[segment->getEndPosition()];
                assert( openEndPoints.count( sibling->getStartPosition()) &&
                        openEndPoints.count( sibling->getEndPosition()));
                openEndPoints.erase( sibling->getStartPosition() );
                openEndPoints.erase( sibling->getEndPosition() );
                ringSegments.push_back( RingSegment(segment, sibling));
                segment = &ringSegments.back();
                //could still be closed in itself or connect to another open segment
                // --> loop again
                continue; 
            }
            
            /* if this point is reached, then 'segment' is not closed in itself,
             * and cannot be connected directly to any RingSegment in openEndPoints.
             * So register its two endpoints as two additional open endpoints*/
             
             openEndPoints.insert( std::make_pair( segment->getStartPosition(), segment));
             openEndPoints.insert( std::make_pair( segment->getEndPosition(), segment));
             break;
        }
    }
    
    void warnUnconnectedNodes(uint64_t relId) {
        if (openEndPoints.size())
        {
            std::cerr << "[WARN] multipolygon relation " << relId << " has unconnected nodes. Will ignore all affected open rings: " << std::endl;
            for ( const std::pair<OsmGeoPosition, RingSegment*> &kv : openEndPoints)
            {
                //printRingSegmentHierarchy( kv.second);
                std::cerr << "\tnode " << kv.first.id << " (" << (kv.first.lat/10000000.0) << "°, " << (kv.first.lng/10000000.0) << "°) ->" << kv.second << std::endl;
            }
        }
    
    }
    
    const std::vector<RingSegment*> &getClosedRings() const { return this->closedRings;}

private:
    /* cannot be a std::vector, because we use pointers into this container,
     * and pointers into a vector are not guaranteed to be stable when adding
     * or removing elements from/to it (causing a resize of the underlying array) */
    std::list<RingSegment> ringSegments;
    std::vector<RingSegment*> closedRings;
    std::map<OsmGeoPosition, RingSegment*> openEndPoints;


};

#endif

