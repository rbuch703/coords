
#include <assert.h>

#include <iostream>
#include <map>
#include <list>
#include <string>
#include <algorithm> //for sort()

#include "osm/osmMappedTypes.h"
#include "geom/ringSegment.h"
#include "geom/ring.h"

using namespace std;

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


bool lessThan( Ring* a, Ring* b)
{
    return a->area > b->area;
}

void addToRingHierarchyRecursive( Ring* ring, vector<Ring*> &rootContainer, uint64_t relId)
{
    for (uint64_t i = 0; i< rootContainer.size(); i++)
    {
        Ring* root = rootContainer[i];
        
        /* note: contains() only means that no part of 'ring' lies outside of 'root'.
                 This is less strict than that the OGC multipolygon requirements (which
                 demand - among others - that the inner ring touches the outer one 
                 at most at a finite number of points (while contains() allows touching
                 edges). But contains() is good enough for now to establish a hierarchy.
                 The edge cases (like touching edges) need to be removed in a later 
                 post-processing step (e.g. through CSG subtraction).*/
        if ( root->contains(*ring) )
            return addToRingHierarchyRecursive( ring, root->children, relId);

        if ( root->interiorIntersectsWith(*ring))
        {
            cerr << "[WARN] overlapping rings found in relation " << relId << ", merging them." << endl;
            cerr << "\tRing 1: ";
            for (uint64_t wayId : root->wayIds)
                cerr << wayId << ", ";
            cerr << endl;

            cerr << "\tRing 2: ";
            for (uint64_t wayId : ring->wayIds)
                cerr << wayId << ", ";
            cerr << endl;

            vector<uint64_t> wayIds( root->wayIds.begin(), root->wayIds.end());
            wayIds.insert( wayIds.end(), ring->wayIds.begin(), ring->wayIds.end() );
            Ring* merged = new Ring( root->geosPolygon->Union(ring->geosPolygon), wayIds);
            
            /* unions cannot reduce the area, and a reduction would corrupt this algorithm,
             * which assumes that rings are inserted sorted descending by area. */
            assert(merged->area >= root->area);
            delete root;
            delete ring;
            
            rootContainer[i] = merged;
            return;
        }
    }

    //no overlap found --> add ring at this level
    rootContainer.push_back(ring);
   

}

vector<Ring*> buildRingHierarchy( vector<Ring*> &sortedRings, uint64_t relId)
{
    assert( is_sorted(sortedRings.begin(), sortedRings.end(), lessThan));
    vector<Ring*> roots;
    
    for ( Ring *ring : sortedRings)
        addToRingHierarchyRecursive( ring, roots, relId);
    
    sortedRings.clear();
    return roots;
}

void deleteRecursive(Ring* ring)
{
    for (Ring *child : ring->children)
        deleteRecursive(child);
    delete ring;
}

int main()
{

    /*OsmGeoPosition pos[] = {
        (OsmGeoPosition){.id = 0, .lat = 0, .lng = 0},
        (OsmGeoPosition){.id = 1, .lat = 1, .lng = 0},
        (OsmGeoPosition){.id = 2, .lat = 2, .lng = 2},
        (OsmGeoPosition){.id = 3, .lat = 0, .lng = 1},
        (OsmGeoPosition){.id = 0, .lat = 0, .lng = 0},
    };
    
    int numVertices = sizeof(pos)/sizeof(OsmGeoPosition);
    Ring ring(vector<OsmGeoPosition>(pos, pos+numVertices), vector<uint64_t>());
    cout << ring.area << endl;
    exit(0);*/

    RelationStore relStore("intermediate/relations");
    LightweightWayStore wayStore("intermediate/ways");
    
    for (const OsmRelation &rel : relStore)
    //int dummy = 0;
    //for (OsmRelation rel = relStore[120651]; dummy == 0; dummy++)
    {
        map<string, string> tags(rel.tags.begin(), rel.tags.end());
        if ((! tags.count("type")) || (tags["type"] != "multipolygon"))
            continue;
        
        /* cannot be a std::vector, because we use pointers into this container,
         * and pointers into a vector are not guaranteed to be stable when adding
         * or removing elements from/to it (causing a resize of the underlying array) */
        list<RingSegment> ringSegments;
        vector<RingSegment*> closedRings;
        
        map<OsmGeoPosition, RingSegment*> openEndPoints;
        
        for ( const OsmRelationMember &mbr : rel.members)
        {
            if (mbr.type != WAY)
            {
                cerr << "[WARN] ref to id=" << mbr.ref << " of invalid type '"<< (mbr.type == NODE ? "node" : mbr.type == RELATION ? "relation" : "<unknown>")<<"' in multipolygon relation " << rel.id << endl;
                continue;
            }
            
            if (! wayStore.exists(mbr.ref))
            {
                cerr << "[WARN] relation " << rel.id << " reference way " << mbr.ref << ", which is not part of the data set" << endl;
                continue;
            }
            
            OsmLightweightWay way = wayStore[mbr.ref];
            if (way.numVertices < 1)
            {
                cerr << "[WARN] way " << way.id << " has no member nodes; ignoring it" << endl;
                continue;
            }
            
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
                 
                 openEndPoints.insert( make_pair( segment->getStartPosition(), segment));
                 openEndPoints.insert( make_pair( segment->getEndPosition(), segment));
                 break;
            }
            
        }

        if (openEndPoints.size())
        {
            cerr << "[WARN] multipolygon relation " << rel.id << " has open way end points. Will ignore all affected open rings: " << endl;
            for ( const pair<OsmGeoPosition, RingSegment*> &kv : openEndPoints)
            {
                cerr << "\tnode " << kv.first.id << " (" << (kv.first.lat/10000000.0) << "°, " << (kv.first.lng/10000000.0) << "°)" << endl;
            }
        }
        
        vector<Ring*> multipolygonRings;
        
        for (RingSegment* rootSegment : closedRings)
        {
            
            vector<OsmGeoPosition> vertices;
            vector<uint64_t>       wayIds;
            
            flatten(rootSegment, wayStore, vertices, wayIds, false);
            if ( vertices.size() < 4)
            {
                cerr << "[WARN] multipolygon ring with less then four vertices in relation " << rel.id << "; skipping it." << endl;
                continue;
            }
            
            multipolygonRings.push_back( new Ring(vertices, wayIds) );
            //cout << ring.area << endl;
            //cout << "flattened " << ring.wayIds.size() << " ways to list of " << ring.vertices.size() << " vertices" << endl;
/*            for (OsmGeoPosition pos : vertices)
            {
                cout << pos.lat << ", " << pos.lng << endl;
            }
            
            exit(0);*/
        }
        
        //cout << "processing relation " << rel.id << endl;
        std::sort( multipolygonRings.begin(), multipolygonRings.end(), lessThan);
        vector<Ring*> roots = buildRingHierarchy( multipolygonRings, rel.id );
        for (Ring* ring : roots)
            deleteRecursive(ring);

        /*for (Ring* ring : multipolygonRings)
            cout << "\tring with area " << ring->area << endl;*/
        //cout << rel.members.size() << ", " << rel << endl;
    }


    return EXIT_SUCCESS;
}
