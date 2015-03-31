
#include <assert.h>

#include "ring.h"


static void flatten(RingSegment *closedRing, LightweightWayStore &ways,
                    std::vector<OsmGeoPosition> &vertices, 
                    std::vector<uint64_t> &wayIds, bool globalReversal = false)
{
    if (closedRing->getWayId() > 0)
    {   //is a leaf node, contains a reference to an actual OSM way.
        
        assert( !closedRing->child1 && !closedRing->child2);
        assert( ways.exists(closedRing->getWayId()) );
        OsmLightweightWay way = ways[closedRing->getWayId()];
        bool effectiveReversal = closedRing->isReversed ^ globalReversal;
        
        wayIds.push_back(way.id);
        
        if (!effectiveReversal)
        {
            uint64_t i = 0;
            if (vertices.size() && vertices.back() == way.vertices[0])
                i += 1; //skip duplicate starting vertex
                
            for (; i < way.numVertices; i++)
                vertices.push_back( way.vertices[i] );
        }
        else
        {
            uint64_t i = way.numVertices;
            if (vertices.size() && vertices.back() == way.vertices[way.numVertices - 1])
                i-= 1; //skip duplicate (effective) starting vertex
                
            for (; i > 0; i--)
                vertices.push_back( way.vertices[i-1]);
        }
        
    } else {
        //is an inner node; has no wayReference, but has two child nodes
        assert( closedRing->child1 && closedRing->child2 );
        bool effectiveReversal = closedRing->isReversed ^ globalReversal;

        if (!effectiveReversal)        
        {
            flatten(closedRing->child1, ways, vertices, wayIds, effectiveReversal);
            flatten(closedRing->child2, ways, vertices, wayIds, effectiveReversal);
        } else
        {   //second child first
            flatten(closedRing->child2, ways, vertices, wayIds, effectiveReversal);
            flatten(closedRing->child1, ways, vertices, wayIds, effectiveReversal);
        }
    }
}


Ring::Ring(RingSegment *rootSegment, LightweightWayStore &ways)
{
    flatten(rootSegment, ways, this->vertices, this->wayIds, false);
}

