
#include <assert.h>

#include <iostream>
#include <map>
#include <list>
#include <string>
#include <algorithm> //for sort()
#include <geos/geom/Polygon.h>

#include "osm/osmMappedTypes.h"
#include "geom/ringSegment.h"
#include "geom/ring.h"

using namespace std;

/* The high-level algorithm to create multipolygons from a multipolygon relation is as follows:
 *     1. ignore all non-way members
 *     2. build geometric rings (node sequences that start and end with the same node) by connecting ways
 *     3. from all rings build a geometric "contains" hierarchy (each ring becomes the child node to that
 *       ring which directly contains it)
 *     4. from the "contains" hierarchy, assign inner/outer roles to form the multipolygon (not yet implemented)
 *     5. import tags from outer ways if necessary (not yet implemented)
 *
 *  Details:
 *  ========
 *
 *  2. Building Geometric Rings:
 *  ------------------
 *  Basic idea: Repeatedly find ways that share at least one endpoint, and connect those ways until none are left. 
 *              Then, discard any connected ways that do not start and end at the same point. Those are not closed
 *              and thus represent invalid parts of a multipolygon geometry. The remaining ways are the rings.
 *
 *  Implementation details:
 *  - In order to connect two ways, one way may need to be reversed to fit. In extreme cases, ways have to be reversed
 *    for each connecting of two ways. This could potentially impact performance negatively. 
 *    This implementation instead
 *    first creates an abstract RingSegment representation for each way, containing only its two end points, a 
 *    reference to the wayId, and a boolean 'reversed' flag storing whether a way needs to be reversed before connecting
 *    it (but without actually performing the reversion). The finding and 'connecting' of ways is then performed only
 *    on the RingSegments: Whenever two RingSegments should be connected, a new RingSegment is created that contains both
 *    original segments as its child, and contains the open end points of its children (i.e. those that are not connected
 *    to each other). The 'reversed' flag of the children is set accordingly. From that point on, the two original
 *    RingSegments are no longer considered for connection to other segments, but only the newly created one is. 
 *    This effectively creates a binary tree of RingSegments for each ring, where the end points of the top-most 
 *    RingSegment are identical (if the hierarchy indeed represents a closed ring).
 *    Once built, this hierarchy is then flattened into the actual ring by traversing it in-order, and adding the
 *    nodes of all ways referenced by traversed RingSegments to the output (along with the list of wayIds for
 *    later algorithms that might need that list). In this process, each way is reversed *at most once*.
 *
 *  - Finding pairs of ways that can be connected would be ineffective if implemented naively (testing each way against
 *    every other way). Instead, we use a dictionary to map open (i.e. unconnected) endpoints to the corresponding
 *    RingSegment and test for connectable RingSegments by checking whether a way's endpoint is present in 
 *    that dictionary.
 *    In detail, the dictionary is initially empty, and RingSegments are added to it incrementally: Each RingSegment is only
 *    added to the dictionary, if none of its endpoints are already in there. Otherwise, the RingSegment
 *    belonging to that endpoint is removed from the dictionary (i.e. both of its endpoints are removed), a new
 *    merged RingSegment is created covering the two segments, and only the new RingSegment is re-considered for 
 *    insertion into the dictionary. Any RingSegment that is closed in itself is never added to the dictionary, but
 *    is directly stored in a list of closed rings. If the set of all rings form a valid multipolygon, the list of open
 *    endpoints will be empty, as all rings have been closed. Otherwise (for invalid geometry), the set of open rings
 *    is discarded. 
 *    This approach should only need O(n log(n) ) time (adding at most O(n) RingSegments to the dictionary, with each
 *    insertion requiring O(log(n)) time) in the number of RingSegments, while the naive approach
 *    requires O(n²) time (testing every way against every other way).
 *
 *
 *    3. Building the 'Contains' Hierarchy
 *    ====================================
 *    Multipolygons consist of 'outer' Rings, and 'holes' inside these Rings. In theory, all OSM multipolygon members 
 *    are correctly tagged with the corresponding 'outer' and 'inner' roles. In practice, however, role tagging can be
 *    wrong, inconsistent or missing, and the geometry itself may be inconsistent (e.g. an 'outer' ring overlapping
 *    with an 'inner' ring. Thus, to guarantee creation of valid multipolygons, role tagging must be ignored. Instead,
 *    geometric inconsistencies must be healed, and the correct roles be reconstructed geometrically.
 *    To that end, a 'contains' hierarchy of rings is created, where each ring can have any number of other rings as
 *    children, and a ring A is a direct parent of another ring B iff. A contains B and there is no other ring C which
 *    is contained in A and in turn contains B. From that hierarchy, the outer/inner roles can easily be determined:
 *    any ring on the top level is an 'outer' ring. Any ring on the next level is an 'inner' ring (a hole in an 'outer'
 *    ring). Then, the roles 'outer' and 'inner' alternate for each additional level (e.g. all rings on the third level
 *    are 'outer' rings, representing a polygon inside a hole of another polygon).
 *    
 *    To build this hierarchy efficiently, it is observed that a ring A can only contain another ring B, if the area
 *    of A is bigger than that of B. Thus, all rings are sorted by area in descending order, and are inserted 
 *    into the hierarchy one by one in that order. So, no ring can contain any ring inserted into the hierarchy before
 *    itself, and it is thus guaranteed that once inserted the position of a ring inside the hierarchy never has to
 *    change.
 *
 *    In detail, a ring is inserted into the hierarchy as follows. Each ring is inserted recursively, starting with
 *    the top level (the list of all top-level rings not contained in any other ring.): at each level the ring under 
 *    consideration may be:
 *    - not overlapping with any other ring at that level. In that case it is inserted into the hierarchy at that level.
 *    - overlap with another ring at that level (i.e. there is some geometry shared by both rings, and some geometry
 *      exclusive to either ring). In that case the multipolygon geometry is inconsistent (as rings must
 *      not overlap). This inconsistency is healed by computing the 'union' of the two overlapping rings as a new ring.
 *      This union is guaranteed to have an area bigger than any of the two original rings, is guaranteed to still be a
 *      ring (if the overlap is at least of dimension 1, i.e. a line or an area), and is guaranteed to still be directly
 *      contained in the parent node. Thus, the two original rings are 
 *      discarded, and the new one is added to the hierarchy at the current level *instead* of the overlapping one 
 *      that was present before.
 *    - be completely contained inside another ring at that level. In that case it is inserted recursively as a child
 *      to that ring.
 *
 *    
 */


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
            /* FIXME: 'merged' could theoretically be a (multi)polygon instead of a simple ring. This would corrupt
             *        later data processing. Make sure that 'merged' is either a single ring, or add its sub-rings
             *        to the hierarchy individually. */
            Ring* merged = NULL;
            //Ring* merged = new Ring( root->geosPolygon->Union(ring->geosPolygon), wayIds);
            cout << "merged geometry is of type " << merged->geosPolygon->getGeometryType() << endl;
            
            
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

void printRingSegmentHierarchy( RingSegment* segment, int depth = 0)
{
    string depthSpaces = "";
    for (int i = 0; i < depth; i++)
        depthSpaces += "  ";
        
    cout << depthSpaces /*<< "segment " << segment*/ << " (way " <<  segment->wayId << ") [" 
         << segment->start.id << "; " << segment->end.id << "]" << endl;
    if (segment->child1)
        printRingSegmentHierarchy(segment->child1, depth+1);
        
    if (segment->child2)
        printRingSegmentHierarchy(segment->child2, depth+1);
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
    //for (OsmRelation rel = relStore[7660]; dummy == 0; dummy++)
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
                cerr << "[WARN] relation " << rel.id << " references way " << mbr.ref << ", which is not part of the data set" << endl;
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
            cerr << "[WARN] multipolygon relation " << rel.id << " has unconnected nodes. Will ignore all affected open rings: " << endl;
            for ( const pair<OsmGeoPosition, RingSegment*> &kv : openEndPoints)
            {
                //printRingSegmentHierarchy( kv.second);
                cerr << "\tnode " << kv.first.id << " (" << (kv.first.lat/10000000.0) << "°, " << (kv.first.lng/10000000.0) << "°) ->" << kv.second << endl;
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
            
            std::vector<geos::geom::Polygon*> polygons = Ring::createSimplePolygons(vertices, rel.id);
            
            for (geos::geom::Polygon* polygon : polygons)
                multipolygonRings.push_back( new Ring(polygon, wayIds) );
        }
        
        //cout << "processing relation " << rel.id << endl;
        std::sort( multipolygonRings.begin(), multipolygonRings.end(), lessThan);
        
        for (Ring* r: multipolygonRings)
            delete r;
        //vector<Ring*> roots = buildRingHierarchy( multipolygonRings, rel.id );
        //for (Ring* ring : roots)
        //    deleteRecursive(ring);
    }


    return EXIT_SUCCESS;
}
