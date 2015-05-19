
#include <assert.h>

#include <iostream>
#include <map>
#include <list>
#include <string>
#include <algorithm> //for sort()
#include <set>
#include <geos/geom/Polygon.h>
//#include <geos/geom/LineString.h>
//#include <geos/geom/LinearRing.h>
#include <geos/geom/IntersectionMatrix.h>

//only to support debugging code
/*#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateSequenceFactory.h>*/

#include "config.h"

#include "containers/bucketFileSet.h"
#include "containers/osmRelationStore.h"

#include "osm/osmMappedTypes.h"
#include "geom/ringSegment.h"
#include "geom/ring.h"
#include "geom/ringAssembler.h"
#include "geom/genericGeometry.h"
#include "geom/geomSerializers.h"
#include "escapeSequences.h"

using namespace std;

typedef map<string, string> TagSet;

/* The high-level algorithm to create multipolygons from a multipolygon relation is as follows:
 *     1. ignore all non-way members
 *     2. build geometric rings (node sequences that start and end with the same node) by
 *        connecting ways
 *     3. from all rings build a geometric "contains" hierarchy (each ring becomes the child
 *        node to that ring which directly contains it)
 *     4. from the "contains" hierarchy, assign inner/outer roles to form the multipolygon (not
 *        yet implemented)
 *     5. import tags from outer ways if necessary
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
 *    This effectively creates a binary tree of RingSegments for each ring, where the end points
 *    of the top-most RingSegment are identical (if the hierarchy indeed represents a closed
 *    ring). Once built, this hierarchy is then flattened into the actual ring by traversing it
 *    in-order, and adding the nodes of all ways referenced by traversed RingSegments to the 
 *    output (along with the list of wayIds - for later algorithms that might need that list).
 *    In this process, each way is reversed *at most once*.
 *
 *  - Finding pairs of ways that can be connected would be ineffective if implemented naively
 *    (testing each way against every other way). Instead, we use a dictionary to map open (i.e.
 *    unconnected) endpoints to the corresponding RingSegment and test for connectable
 *    RingSegments by checking whether a way's endpoint is present in that dictionary.
 *    In detail, the dictionary is initially empty, and RingSegments are added to it
 *    incrementally: Each RingSegment is only added to the dictionary, if none of its endpoints
 *    is already in there. Otherwise, the RingSegment belonging to that endpoint is removed from
 *    the dictionary (i.e. both of its endpoints are removed), a new merged RingSegment is
 *    created covering the two segments, and only the new RingSegment is re-considered for 
 *    insertion into the dictionary. Any RingSegment that is closed in itself is never added to
 *    the dictionary, but is directly stored in a list of closed rings. If the set of all rings
 *    form a valid multipolygon, the list of open endpoints will be empty, as all rings have
 *    been closed. Otherwise (for invalid geometry), the set of open rings is discarded. 
 *    This approach should only need O(n log(n) ) time (adding at most O(n) RingSegments to the
 *    dictionary, with each insertion requiring O(log(n)) time) in the number of RingSegments,
 *    while the naive approach requires O(n²) time (testing every way against every other way).
 *
 *
 *    3. Building the 'Contains' Hierarchy
 *    ====================================
 *    Multipolygons consist of 'outer' Rings, and 'holes' inside these Rings. In theory, all OSM
 *    multipolygon members are correctly tagged with the corresponding 'outer' and 'inner'
 *    roles. In practice, however, role tagging can be wrong, inconsistent or missing, and the
 *    geometry itself may be inconsistent (e.g. an 'outer' ring overlapping with an 'inner'
 *    ring). Thus, to guarantee creation of valid multipolygons, role tagging must be ignored.
 *    Instead, geometric inconsistencies must be healed, and the correct roles be reconstructed
 *    geometrically.
 *    To that end, a 'contains' hierarchy of rings is created, where each ring can have any
 *    number of other rings as children, and a ring A is a direct parent of another ring B iff.
 *    A contains B and there is no other ring C which is contained in A and in turn contains B.
 *    From that hierarchy, the outer/inner roles can easily be determined: any ring on the top
 *    level is an 'outer' ring. Any ring on the next level is an 'inner' ring (a hole in an
 *    'outer' ring). Then, the roles 'outer' and 'inner' alternate for each further level
 *    (e.g. all rings on the third level are 'outer' rings, representing a polygon inside a 
 *    hole of another polygon).
 *    
 *    To build this hierarchy efficiently, it is observed that a ring A can only contain another
 *    ring B, if the area of A is bigger than that of B. Thus, all rings are sorted by area in
 *    descending order, and are inserted into the hierarchy one by one in that order. So, no
 *    ring can contain any ring inserted into the hierarchy beforeitself, and it is thus
 *    guaranteed that once inserted the position of a ring inside the hierarchy never has to
 *    change.
 *
 *    In detail, a ring is inserted into the hierarchy as follows. Each ring is inserted
 *    recursively, starting with the top level (the list of all top-level rings not contained in
 *    any other ring.): at each level the ring under consideration may be:
 *    - not overlapping with any other ring at that level. In that case it is inserted into 
 *      the hierarchy at that level.
 *    - overlap with another ring at that level (i.e. there is some geometry shared by both
 *      rings, and some geometry exclusive to either ring). In that case the multipolygon
 *      geometry is inconsistent (as rings must not overlap). This inconsistency is healed by
 *      computing the 'union' of the two overlapping rings as a new ring. This union is
 *      guaranteed to have an area bigger than any of the two original rings, and is guaranteed
 *      to still be a ring (if the overlap is at least of dimension 1, i.e. a line or an area),
 *      and is guaranteed to still be directly contained in the parent node. Thus, the two
 *      original rings are discarded, and the new one is added to the hierarchy at the current
 *      level *instead* of the overlapping one that was present before.
 *    - be completely contained inside another ring at that level. In that case it is inserted
 *      recursively as a child to that ring.
 */



static void flattenRingSegmentHierarchy(const RingSegment &closedRing, map<uint64_t, OsmLightweightWay> &ways,
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

        flattenRingSegmentHierarchy ( globalReversal ? *closedRing.getSecondChild(): *closedRing.getFirstChild(),  ways, verticesOut, wayIds, effectiveReversal);
        flattenRingSegmentHierarchy ( globalReversal ? *closedRing.getFirstChild() : *closedRing.getSecondChild(), ways, verticesOut, wayIds, effectiveReversal);
    } else
    {
        MUST(false, "invalid wayId");
    }
}


bool hasBiggerArea ( Ring* a, Ring* b) { return a->getArea() > b->getArea(); }
bool hasSmallerArea( Ring* a, Ring* b) { return a->getArea() < b->getArea(); }

#if 0
void printPolygon(const geos::geom::Geometry *geo)
{
    assert(geo->getGeometryTypeId() == geos::geom::GEOS_POLYGON);
    const geos::geom::Polygon *poly = dynamic_cast<const geos::geom::Polygon*>(geo);
    static const double xOffset = 0; // 4.994E8
    static const double yOffset = 0; // 8.593E7
    {
        auto coords = poly->getExteriorRing()->getCoordinates();
        for (uint64_t i = 0; i < coords->getSize(); i++)
            cerr << ((*coords)[i].x- xOffset) << ", " << ((*coords)[i].y-yOffset) << endl;
        delete coords;
    }
    for (uint64_t i = 0; i < poly->getNumInteriorRing(); i++)
    {
        cerr << "interior ring " << i << endl;
        
        
        const geos::geom::LineString *ring = poly->getInteriorRingN(i);
        auto coords = ring->getCoordinates();
        for (uint64_t i = 0; i < coords->getSize(); i++)
            cerr << ((*coords)[i].x - xOffset) << ", " << ((*coords)[i].y - yOffset) << endl;
        delete coords;
    }
}
#endif

vector<Ring*> buildRingHierarchy( vector<Ring*> &rings, uint64_t relId)
{
    std::sort( rings.begin(), rings.end(), hasBiggerArea);

    vector<Ring*> roots;
    
    for ( Ring *ring : rings)
        ring->insertIntoHierarchy( roots, relId);
    
    /*  insertIntoHierarchy() takes ownership of all rings passed to it:
        they are either incorporated into the hierarchy, or deleted. So at this point, 
        rings is just an array of stale pointers, and can (and should!) be cleared. */
    rings.clear();
    return roots;
}

template <typename T> T removeAt(vector<T> &v, uint64_t pos)
{
    assert( pos < v.size() && "overflow");
    T res = v[pos];
    v[pos] = v[v.size()-1];
    v.pop_back();
    return res;
}

std::ostream& operator<<( std::ostream &os, const std::vector<uint64_t> &nums)
{
    for (uint64_t i = 0; i < nums.size(); i++)
    {
        if (i > 0)
            os << ", ";
        os << nums[i];
    }

    return os;
}

std::ostream& operator<<( std::ostream &os, const std::vector<Ring*> &rings)
{
    for (Ring* r : rings)
        os << "(" << r->wayIds << "), ";
    return os;
}



void joinAdjacentRings( vector<Ring*> &rings, uint64_t relId)
{
    /* FIXME: currently, rings are tested against each other essentially in random order. 
     *        This can likely be sped up considerably for large multipolygons:
     *        1. first test all small rings against each other, and merge/subtract them as
     *           necessary; only afterwards do the same also for the large rings. This should
     *           reduce the number of expensive merge operations on large rings (instead of
     *           N mergers for N small rings with a big one, this may jsut perform N-1 mergers
     *           for small/small rings, and a single large/small operation.
     *        2. create prepared geometries for all rings. First perform all intersection/
     *           overlap tests between all prepared geometries (should be fast) and just note
     *           the yes/no results. Afterwards performed the CSG union/difference operations
     *           based on these results. This ensures that only a limited number of prepared
     *           geometries have to be created (as no prepared geometries for merged polygons
     *           have to be created).
     *        3. perform early-termination intersection tests based on the geometry's envelopes.
     *           (could be extended by creating a quadtree hierarchy of envelopes to reduce the
     *           total number envelope/envelope geometric tests that have to be performed. But
     *           since almost no multipolygon has more than ~1000 rings, the envelope tests are
     *           probably fast enough, and this approach will not further reduce the number of
     *           actual polygon/polygon tests than have to be performed).
     */
    
    uint64_t pos = 0;
    
    while (pos < rings.size() )
    {
        bool ringJoined = false;
        
        for (uint64_t pos2 = pos+1; pos2 < rings.size(); pos2++)
        {
            MUST(rings[pos] != rings[pos2], "attempt to compare ring to itself");
            geos::geom::IntersectionMatrix *mat = rings[pos]->getPolygon()->relate(rings[pos2]->getPolygon());
            /* are adjacent if their interiors overlap, or if their boundaries touch
             * in at least 1 dimension (i.e. a line) */
            bool areAdjacentOrOverlap = mat->isOverlaps(2, 2) || mat->get(1,1) >= 1;
            bool identical = mat->isEquals(2,2);
            
            if (identical)
            {
                Ring* ring =  rings[pos];
                Ring* ring2 = removeAt(rings, pos2);
                cerr << "[WARN] in relation " << relId << ": identical rings (ways " 
                     << (ring->wayIds) << ") and (ways " << (ring2->wayIds) << ")" << endl;
                ring->wayIds.insert(ring->wayIds.end(), ring2->wayIds.begin(), ring2->wayIds.end());
                pos2--;
                delete ring2;
                delete mat;
                continue;
            }
            if (!areAdjacentOrOverlap)
            {
                delete mat;
                continue;
            }
            
            //cerr <<  rings << endl;
            ringJoined = true;
            
            /* if this point is reached, the rings will be joined and the original rings
             * removed from the set. So remove them right away.
             * Note that the rings have to be extracted in that order: pos2 is bigger than pos,
             * and removeAt removes an array entry by replacing it with the last one and deleting
             * the last entry. So if pos2 is the last array index, removing the entry at pos
             * would move the entry at pos2 to a new position, after which pos2 points at an
             * invalid element */
            Ring* ring2 = removeAt(rings, pos2);
            Ring* ring = removeAt(rings, pos);
            
            for (Ring* mergedRing: Ring::merge(ring, ring2, mat, relId))
                rings.push_back(mergedRing);
                
            delete mat;
            delete ring;
            delete ring2;
                
            break;
        }
        
        if (!ringJoined)
            pos++;
    }
}

/* This method heuristically adds tags to multipolygons based on the tags of its outer ways.
   Theoretically, all multipolgons should be tagged directly at their MultiPolygon relation.
   However, an old tagging scheme tagged the outer way of the relation instead. And some
   workflows (e.g. adding a hole to an area that was formerly represented by a single way)
   can also lead to outer ways being tagged instead of the whole multipolygon relation.
*/
TagSet getMultipolygonTags( Ring* ring, const OsmRelation &rel, const map<uint64_t, 
                            OsmLightweightWay> &ways, const TagSet &outerTags)
{
    TagSet tags;
    for (const OsmKeyValuePair &kv : rel.tags)
        tags.insert(make_pair(kv.first, kv.second));
        
    MUST(tags.count("type") && tags.at("type") == "multipolygon", "not a multipolygon");
    MUST(ring->wayIds.size() > 0, "not a multipolygon");
    
    tags.erase("type"); // type=multipolygon is implicit for all multipolygons
    
    /* the "valid" case: the relation itself is tagged --> just use those tags*/
    if (tags.size() > 0)
        return tags;

    /* heuristic 1: relation is untagged, but the polygon outer ring consists of only one way
     *              --> use the tags from that way.
     */
    if (ring->wayIds.size() == 1)
    {
        uint64_t outerWayId = ring->wayIds.front();
        /* This is the case for about 10% of all multipolygons. Its currently too prevalent
         * to warn or even just inform about it. */
        /*cerr << ESC_FG_GRAY << "[INFO] relation " << rel.id << " has no tags. " <<
               << "Will use tags from" << " outer way " << outerWayId << "."
               << ESC_FG_RESET  << endl;*/
             
        MUST( ways.count(outerWayId) == 1, "cannot access outer way");
        
        const OsmLightweightWay &way = ways.at(outerWayId);
        for (const OsmKeyValuePair &kv : way.getTags())
            tags.insert( make_pair(kv.first, kv.second));
        
        return tags;
    }
    
    /* heuristic 2: relation is untagged, but only one of its ways is tagged with the 'outer'
     *              role - though this way need not actually form the outer ring --> 
     *              --> use tags from that way */
    if (outerTags.size() > 0)
    {
        for (const OsmKeyValuePair &kv : outerTags)
            tags.insert( make_pair(kv.first, kv.second));
        
        return tags;
        
    }
    
    /* heuristic 3: relation is untagged, and no single way is used or tagged as the only
     *              'outer' way --> use all those tags that all outer ways have in common.
     **/

    vector<TagSet> outerWayTagSets;
    for (uint64_t outerWayId : ring->wayIds)
    {
        MUST( ways.count(outerWayId) > 0, "cannot access outer way");

        TagSet ts;
        for (const OsmKeyValuePair &kv : ways.at(outerWayId).getTags())
            ts.insert( make_pair(kv.first, kv.second));
        
        outerWayTagSets.push_back(ts);
    }

    MUST(outerWayTagSets.size() > 0, "no outer way");
    
    for (OsmKeyValuePair kv : outerWayTagSets.front())
    {
        bool presentInAllOuterWays = true;
        for (const TagSet &ts : outerWayTagSets)
            presentInAllOuterWays &= (ts.count(kv.first) && ts.at(kv.first) == kv.second);
        
        if (presentInAllOuterWays)
            tags.insert(kv);
    }
    
    if (tags.size())
        cerr << ESC_FG_YELLOW << "[WARN] relation " << rel.id << " has no tags. Using those tags"
             << " that its outer ways agree on." << ESC_FG_RESET  << endl;
     else
        cerr << ESC_FG_RED << "[WARN] relation " << rel.id << " has no tags even after heuristic"
             << " members tag-merging." << ESC_FG_RESET << endl;

    return tags;
}

int main()
{
    FILE* fOut = fopen("intermediate/multipolygons.bin", "wb");
    MUST( fOut, "cannot open output file");
    
    FILE* fOuterWayIds = fopen("intermediate/outerWayIds.bin", "wb");
    MUST( fOuterWayIds, "cannot open output file");

    RelationStore relStore("intermediate"/*"/multipolygon_world"*/"/relations");
    
    BucketFileSet<int> relationWaysBuckets("intermediate"/*"/multipolygon_world"*/"/referencedWays",  WAYS_OF_RELATIONS_BUCKET_SIZE, true);

    //for (uint64_t bucketId = 0; bucketId == 0; bucketId++)
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

        //for( uint64_t relId = 37436; relId == 37436; relId++)
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
            
            TagSet outerTags;
            RingAssembler ringAssembler = RingAssembler::fromRelation(rel, ways, outerTags);
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
                
                flattenRingSegmentHierarchy(*rootSegment, ways, vertices, wayIds, false);
                if ( vertices.size() < 4)
                {
                    
                    cerr << ESC_FG_YELLOW << "[WARN] multipolygon ring in relation " << rel.id 
                         << " based on ways (" << wayIds << ") has less then four vertices;"
                         << " skipping it." << ESC_FG_RESET <<endl;
                    continue;
                }
                
                std::vector<geos::geom::Polygon*> polygons = Ring::createSimplePolygons(vertices, rel.id);
                
                for (geos::geom::Polygon* polygon : polygons)
                    multipolygonRings.push_back( new Ring(polygon, wayIds) );
            }
            
            joinAdjacentRings(multipolygonRings, rel.id);
            //for (Ring* r: multipolygonRings)
            //    delete r;
            //cout << "processing relation " << rel.id << endl;
            
            vector<Ring*> roots = buildRingHierarchy( multipolygonRings, rel.id );
            Ring::flattenHierarchyToPolygons(roots);
            
            for (Ring* poly: roots)
            {
                TagSet tags = getMultipolygonTags(poly, rel, ways, outerTags);
                serializePolygon(*poly, TTags(tags.begin(), tags.end()), rel.id, fOut);
                
                for (uint64_t wayId : poly->wayIds)
                    MUST( fwrite(&wayId, sizeof(wayId), 1, fOuterWayIds) == 1, "write error");
            }
            //removeBoundaryOverlaps(roots, rel.id);
            for (Ring* ring : roots)
                Ring::deleteRecursive(ring);
        }
    }
    fclose(fOut);
    fclose(fOuterWayIds);
   
    return EXIT_SUCCESS;
}
