
#include <assert.h>

#include <iostream>
#include <map>
#include <list>
#include <string>
#include <algorithm> //for sort()
#include <geos/geom/Polygon.h>
#include <geos/geom/CoordinateSequence.h>

#include "osm/osmMappedTypes.h"
#include "containers/bucketFileSet.h"
#include "geom/ringSegment.h"
#include "geom/ring.h"
#include "geom/ringAssembler.h"
#include "escapeSequences.h"
#include "config.h"

using namespace std;

/* The high-level algorithm to create multipolygons from a multipolygon relation is as follows:
 *     1. ignore all non-way members
 *     2. build geometric rings (node sequences that start and end with the same node) by
 *        connecting ways
 *     3. from all rings build a geometric "contains" hierarchy (each ring becomes the child
 *        node to that ring which directly contains it)
 *     4. from the "contains" hierarchy, assign inner/outer roles to form the multipolygon (not
 *        yet implemented)
 *     5. import tags from outer ways if necessary (not yet implemented)
 *
 *  Details:
 *  ========
 *
 *  2. Building Geometric Rings:
 *  ------------------
 *  Basic idea: Repeatedly find ways that share at least one endpoint, and connect those ways 
 *              until none are left. Then, discard any connected ways that do not start and
 *              end at the same point. Those are not closed and thus represent invalid parts 
 *              of a multipolygon geometry. The remaining ways are the rings.
 *
 *  Implementation details:
 *  - In order to connect two ways, one way may need to be reversed to fit. In extreme cases, 
 *    ways have to be reversed for each connecting of two ways. This could potentially impact 
 *    performance negatively. 
 *    This implementation instead first creates an abstract RingSegment representation for each 
 *    way, containing only its two end points, a reference to the wayId, and a boolean 'reversed'
 *    flag storing whether a way needs to be reversed before connecting it (but without actually
 *    performing the reversion). The finding and 'connecting' of ways is then performed only on
 *    the RingSegments: Whenever two RingSegments should be connected, a new RingSegment is 
 *    created that contains both original segments as its childred, and contains the open end 
 *    points of its children (i.e. those that are not connected to each other). The 'reversed' 
 *    flag of the children is set accordingly. From that point on, the two original RingSegments
 *    are no longer considered for connection to other segments, but only the newly created one is. 
 *    This effectively creates a binary tree of RingSegments for each ring, where the end points of
 *    the top-most RingSegment are identical (if the hierarchy indeed represents a closed ring).
 *    Once built, this hierarchy is then flattened into the actual ring by traversing it in-order, 
 *    and adding the nodes of all ways referenced by traversed RingSegments to the output (along 
 *    with the list of wayIds - for later algorithms that might need that list). In this process, 
 *    each way is reversed *at most once*.
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



static void flatten(const RingSegment &closedRing, map<uint64_t, OsmLightweightWay> &ways,
                    std::vector<OsmGeoPosition> &verticesOut, 
                    std::vector<uint64_t> &wayIds, bool globalReversal = false)
{
    if (closedRing.getWayId() > 0)
    {   //is a leaf node, contains a reference to an actual OSM way.
        
        assert( !closedRing.getFirstChild() && !closedRing.getSecondChild());
        assert( ways.count(closedRing.getWayId()) );
        OsmLightweightWay way = ways.at(closedRing.getWayId());
        MUST( way.numVertices > 0, "way without nodes");

        bool effectiveReversal = closedRing.isReversed() ^ globalReversal;
        
        wayIds.push_back(way.id);
        
        if (!effectiveReversal)
        {
            uint64_t i = 0;
            MUST( !verticesOut.size() || (verticesOut.back() == way.vertices[0]), "trying to connect segments that do not share an endpoint");
            if (verticesOut.size() && verticesOut.back() == way.vertices[0])
                i += 1; //skip duplicate starting vertex
                
            for (; i < way.numVertices; i++)
                verticesOut.push_back( way.vertices[i] );
        }
        else
        {
            uint64_t i = way.numVertices;
            MUST( !verticesOut.size() || (verticesOut.back() == way.vertices[way.numVertices-1]), "trying to connect segments that do not share an endpoint");

            if (verticesOut.size() && verticesOut.back() == way.vertices[way.numVertices - 1])
                i-= 1; //skip duplicate (effective) starting vertex
                
            for (; i > 0; i--)
                verticesOut.push_back( way.vertices[i-1]);
        }
        
    } else if (closedRing.getWayId() == -2)
    {   /* is an 'artificial' RingSegment not based on an actual OSM way, and just introduced to
         * connect two 'actual' RingSegments that do not share a common endpoint. This happens
         * when a broken multipolygon ring is not closed in OSM, but the opening is small enough
         * to be closed heuristically. 
         * Such an artificial RingSegment consists only of its two endpoints, without any
         * additional vertices.
         */
        
        assert( !closedRing.getFirstChild() && !closedRing.getSecondChild());
        
        MUST( (verticesOut.size() == 0) || (verticesOut.back() == 
              (globalReversal ? closedRing.getEndPosition() : closedRing.getStartPosition())),
               "trying to connect segments that do not share an endpoint");

        verticesOut.push_back( globalReversal ? closedRing.getStartPosition() : closedRing.getEndPosition() );
        
    }    
    else if (closedRing.getWayId() == -1){
        //is an inner node; has no wayReference, but has two child nodes
        assert( closedRing.getFirstChild() && closedRing.getSecondChild());
        bool effectiveReversal = closedRing.isReversed() ^ globalReversal;

        flatten ( globalReversal ? *closedRing.getSecondChild(): *closedRing.getFirstChild(),  ways, verticesOut, wayIds, effectiveReversal);
        flatten ( globalReversal ? *closedRing.getFirstChild() : *closedRing.getSecondChild(), ways, verticesOut, wayIds, effectiveReversal);
    } else
    {
        MUST(false, "invalid wayId");
    }
}


bool hasBiggerArea( Ring* a, Ring* b)
{
    return a->area > b->area;
}

void printPolygon(const geos::geom::Geometry *geo)
{
    assert(geo->getGeometryTypeId() == geos::geom::GEOS_POLYGON);
    auto coords = geo->getCoordinates();
    for (uint64_t i = 0; i < coords->getSize(); i++)
        cerr << ((*coords)[i].x+ 3.49E8) << ", " << ((*coords)[i].y - 1.386E9) << endl;
    
    
    delete coords;
}

void addToRingHierarchyRecursive( Ring* ring, vector<Ring*> &rootContainer, uint64_t relId)
{
    for (uint64_t i = 0; i< rootContainer.size(); i++)
    {
        Ring* root = rootContainer[i];
        
        /* note: contains() only means that no part of 'ring' lies outside of 'root'.
                 This is less strict than that the OGC multipolygon requirements (which
                 demand - among others - that the inner ring touches the outer one 
                 at most at a finite number of points, while contains() allows touching
                 edges). But contains() is good enough for now to establish a hierarchy.
                 The edge cases (like touching edges) need to be removed in a later 
                 post-processing step (e.g. through CSG subtraction).*/
        if ( root->contains(*ring) )
            return addToRingHierarchyRecursive( ring, root->children, relId);

        if ( root->interiorIntersectsWith(*ring))
        {
            cerr << ESC_FG_YELLOW << "[WARN] overlapping rings found in relation " << relId << ", merging them." << ESC_FG_RESET << endl;
            cerr << "\tRing 1: ways ";
            for (uint64_t wayId : root->wayIds)
                cerr << wayId << ", ";
            cerr << endl;

            cerr << "\tRing 2: ways ";
            for (uint64_t wayId : ring->wayIds)
                cerr << wayId << ", ";
            cerr << endl;

            vector<uint64_t> wayIds( root->wayIds.begin(), root->wayIds.end());
            wayIds.insert( wayIds.end(), ring->wayIds.begin(), ring->wayIds.end() );

            cerr << ESC_FG_RED << "Merging overlapping rings in relation " << relId 
                 << ESC_FG_RESET << endl;
            geos::geom::Geometry *merged = root->geosPolygon->Union(ring->geosPolygon);
            MUST( merged->getGeometryTypeId() == geos::geom::GEOS_POLYGON, "merge error");

            /* unions cannot reduce the area, and a reduction would void this algorithm, as
             * it assumes that rings are inserted sorted descending by area. We still allow
             * for a very small relative reduction to account for numerical inaccuracies */
            double relativeArea = merged->getArea() / root->area;
            MUST( relativeArea >= 0.99999999999999 , "union reduced area");

            Ring *mergedRing = new Ring( dynamic_cast<geos::geom::Polygon*>(merged), wayIds);
            MUST( ring->children.size() == 0, "simple ring cannot have children");
            mergedRing->children = root->children;

            /* These assertions should be true geometrically, but can fail in GEOS due to
             * numerical inaccuracies */
            /*MUST( merged->covers(root->geosPolygon), "merge error");
            MUST( merged->covers(ring->geosPolygon), "merge error");*/
            

            delete root;
            delete ring;
            
            rootContainer[i] = mergedRing;
            return;
        }
    }

    //no overlap found --> add ring at this level
    rootContainer.push_back(ring);
   

}

vector<Ring*> buildRingHierarchy( vector<Ring*> &sortedRings, uint64_t relId)
{
    assert( is_sorted(sortedRings.begin(), sortedRings.end(), hasBiggerArea));
    vector<Ring*> roots;
    
    for ( Ring *ring : sortedRings)
        addToRingHierarchyRecursive( ring, roots, relId);
    
    /*  addToRingHierarchyRecursive() takes ownership of all rings passed to it:
        they are either incorporated into the hierarchy, or deleted. So at this point, 
        sortedRings is just an array of stale pointers, and can (and should!) be cleared. */
    sortedRings.clear();
    return roots;
}

template <typename T> void removeAt(vector<T> &v, uint64_t pos)
{
    v[pos] = v[v.size()-1];
    v.pop_back();
}


void replaceByDifferencePolygon( vector<Ring*> &root, uint64_t parentPos, uint64_t childPos, geos::geom::Geometry* diff)
{
    Ring* parent = root[parentPos];
    Ring* child  = parent->children[childPos];
    /* computes the geometric difference of 'parent' and 'child', 
     * and replaces both by that difference. */
    //MUST(false, "untested branch");
    geos::geom::Polygon *diffPoly = dynamic_cast<geos::geom::Polygon*>(diff);
    /* All ring hierarchy algorithms used here assume that each element is
     * a geometric ring, i.e. a simple polygon without holes. So the result
     * of the geometric difference has to be a ring as well. And since both
     * input geometries to the difference operation were rings whose outlines
     * overlap in at least a single point and where one is completely 
     * contained in the other, the result should always be a ring as well. */
    MUST( diffPoly->getNumInteriorRing() == 0, "invalid difference result");
    
    vector<uint64_t> wayIds = parent->wayIds;
    wayIds.insert(wayIds.end(), child->wayIds.begin(), child->wayIds.end());
    Ring* diffRing = new Ring(diffPoly, wayIds);
    
    diffRing->children = parent->children;
    /* remove child (which is going to be deleted) from list of children*/
    removeAt( diffRing->children, childPos);
    delete parent;
    
    root.insert(root.end(), child->children.begin(), child->children.end());
    delete child;
    
    root[parentPos] = diffRing;
}
void removeBoundaryOverlaps( vector<Ring*> &root, uint64_t relId)
{
    uint64_t parentPos = 0;
    while (parentPos < root.size())
    {
        Ring* parent = root[parentPos];
        bool parentHasChanged = false;
        
        for (uint64_t childPos = 0; childPos < parent->children.size(); childPos++)
        {
//            MUST(false, "untested branch");

            Ring* child = parent->children[childPos];
            MUST( parent->contains(*child), "inclusion hierarchy violation");

            if (!Ring::boundariesTouch(*parent, *child))
                continue;

            cerr << ESC_FG_YELLOW << "[Warn] in relation " << relId << ": child ring (ways ";
            for (uint64_t wayId : child->wayIds)
                cerr << wayId << ", ";
            cerr << ") touches parent ring (ways ";
            for (uint64_t wayId : parent->wayIds)
                cerr << wayId << ", ";
            cerr << "). Computing geometric difference polygon." << ESC_FG_RESET << endl;
            
            

            

            geos::geom::Geometry *diff = parent->geosPolygon->difference( child->geosPolygon);
            
            switch (diff->getGeometryTypeId())
            {
                case geos::geom::GEOS_POLYGON:
                {
                    replaceByDifferencePolygon(root, parentPos, childPos, diff);
                    parentHasChanged = true;
                    break;
                }
                case geos::geom::GEOS_MULTIPOLYGON:
                    MUST(false, "untested branch");
                    for (uint64_t i = 0; i < diff->getNumGeometries(); i++)
                        MUST( diff->getGeometryN(i)->getGeometryTypeId() == geos::geom::GEOS_POLYGON, "unknown result of geometric difference");
                    break;
                default:
                    MUST(false, "unknown result of geometric difference");
                    break;
            } //switch
            
        } //for childPos
        
        if (!parentHasChanged)
            parentPos++;
    }
}

void deleteRecursive(Ring* ring)
{
    for (Ring *child : ring->children)
        deleteRecursive(child);
    delete ring;
}

bool isValidWay(OsmRelationMember mbr, uint64_t relationId, const map<uint64_t, OsmLightweightWay> &ways)
{
    if (mbr.type != OSM_ENTITY_TYPE::WAY)
    {
        cerr << ESC_FG_GRAY << "[INFO]" << " ref to id=" << mbr.ref << " of invalid type '"
             << (mbr.type == OSM_ENTITY_TYPE::NODE ? "node" : 
                 mbr.type == OSM_ENTITY_TYPE::RELATION ? "relation" : "<unknown>"
                )
             << "' in multipolygon relation " << relationId << ESC_FG_RESET << endl;
        return false;
    }
    
    if (! ways.count(mbr.ref))
    {
        cerr << "[WARN] relation " << relationId << " references way " << mbr.ref 
             << ", which is not part of the data set" << endl;
        return false;
    }
    

    if (ways.at(mbr.ref).numVertices < 1)
    {
        cerr << "[WARN] way " << mbr.ref << " has no member nodes; ignoring it" << endl;
        return false;
    }
    
    return true;
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

    RelationStore relStore("intermediate"/*"/multipolygon_world"*/"/relations");
    
    BucketFileSet<int> relationWaysBuckets("intermediate"/*"/multipolygon_world"*/"/referencedWays",  WAYS_OF_RELATIONS_BUCKET_SIZE, true);

    //for (uint64_t bucketId = 16; bucketId == 16; bucketId++)
    for (uint64_t bucketId = 0; bucketId < relationWaysBuckets.getNumBuckets(); bucketId++)
    {
        std::cout << "reading way bucket " << (bucketId+1) << "/" << relationWaysBuckets.getNumBuckets() << std::endl;
        FILE* f = relationWaysBuckets.getFile(bucketId* WAYS_OF_RELATIONS_BUCKET_SIZE);
        MUST(f != nullptr, "cannot open bucket file");
        
        map<uint64_t, OsmLightweightWay> ways;
        
        int ch;
        while ( (ch = fgetc(f)) != EOF )
        {
            ungetc(ch, f);
            OsmLightweightWay way(f);
            ways.insert(make_pair( way.id, way));
        }

        //for( uint64_t relId = 1698122; relId == 1698122; relId++)
        /*for (uint64_t relId =  1698122 > bucketId   * WAYS_OF_RELATIONS_BUCKET_SIZE ?
                               1698122 : bucketId   * WAYS_OF_RELATIONS_BUCKET_SIZE;
                      relId < (bucketId+1)* WAYS_OF_RELATIONS_BUCKET_SIZE;
                      relId++)*/

        for (uint64_t relId =  bucketId   * WAYS_OF_RELATIONS_BUCKET_SIZE;
                      relId < (bucketId+1)* WAYS_OF_RELATIONS_BUCKET_SIZE;
                      relId++)
        {
            if (! relStore.exists(relId))
                continue;
                
            OsmRelation rel = relStore[relId];
        
            map<string, string> tags(rel.tags.begin(), rel.tags.end());
            if ((! tags.count("type")) || (tags["type"] != "multipolygon"))
                continue;
            
            RingAssembler ringAssembler;                        
            
            for ( const OsmRelationMember &mbr : rel.members)
                if (isValidWay( mbr, rel.id, ways))
                    ringAssembler.addWay(ways[mbr.ref]);

            ringAssembler.warnUnconnectedNodes(rel.id);
            
            if (ringAssembler.hasOpenRingSegments())
            {
                double diameter = ringAssembler.getAABBDiameter(ways);
                ringAssembler.tryCloseOpenSegments( diameter / 10.0);
                if (ringAssembler.hasOpenRingSegments())
                    cerr << ESC_FG_YELLOW << "\tnot all open rings could be closed "
                         << "heuristically. Will ignore any ring that is still open" 
                         << ESC_FG_RESET << endl;
            }
            
            vector<Ring*> multipolygonRings;
            
            for (RingSegment* rootSegment : ringAssembler.getClosedRings())
            {
                
                vector<OsmGeoPosition> vertices;
                vector<uint64_t>       wayIds;
                
                flatten(*rootSegment, ways, vertices, wayIds, false);
                if ( vertices.size() < 4)
                {
                    
                    cerr << ESC_FG_YELLOW << "[WARN] multipolygon ring in relation " << rel.id 
                         << " based on ways (";
                    for (uint64_t wayId : wayIds)
                        cerr << wayId << ", ";
                    
                    cerr << ") has less then four vertices ; skipping it." << ESC_FG_RESET <<endl;
                    continue;
                }
                
                std::vector<geos::geom::Polygon*> polygons = Ring::createSimplePolygons(vertices, rel.id);
                
                for (geos::geom::Polygon* polygon : polygons)
                    multipolygonRings.push_back( new Ring(polygon, wayIds) );
            }
            
            //cout << "processing relation " << rel.id << endl;
            std::sort( multipolygonRings.begin(), multipolygonRings.end(), hasBiggerArea);
            
            /*for (Ring* r: multipolygonRings)
                delete r;*/
            vector<Ring*> roots = buildRingHierarchy( multipolygonRings, rel.id );
            removeBoundaryOverlaps(roots, rel.id);
            for (Ring* ring : roots)
                deleteRecursive(ring);
        }
    }


    return EXIT_SUCCESS;
}
