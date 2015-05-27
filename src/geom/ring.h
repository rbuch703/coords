
#ifndef RING_H
#define RING_H

#include <vector>

#include <geos/geom/GeometryFactory.h>

#include "osm/osmTypes.h"
#include "geom/ringSegment.h"



class Ring {
public:
    // note: 'Ring' takes ownership of 'geosPolygon', and handles its deletion
    Ring(geos::geom::Polygon *geosPolygon, const std::vector<uint64_t> wayIds);

    ~Ring();
    double getArea() const;
    bool overlapsWith(const Ring &other) const;

#if 0    
    /* returns true iff 'other' lies completely in the *interior* of 'this', 
       i.e. the boundaries of both rings do not meet or overlap.*/
    bool containsInInterior(const Ring &other) const;


    /* returns true iff 'other' follows the rules of an inner ring:
     * no point of it lies outside of 'this', and the boundaries
     * of 'this' and 'other' share at most a finite number of points
     * (no edges).*/
    bool containsAsInner(const Ring &other) const;
#endif


    bool contains(const Ring &other) const;
    
    static bool boundariesTouch(const Ring &a, const Ring &b);

    
    /* returns true iff 'this' and 'other' have at least one point in common
       that does not lie on the boundary or either ring. */
    bool interiorIntersectsWith(const Ring &other) const;
    void insertIntoHierarchy( std::vector<Ring*> &hierarchyRoot, uint64_t relId);

    void serialize(FILE* fOut, bool reverseVertexOrder) const;
    uint64_t getSerializedSize(bool reverseVertexOrder) const;
    const geos::geom::Polygon* getPolygon() const;
    
    //const std::vector<uint64_t> & getWayIds() const { return wayIds;}
//private:
public:
    std::vector<Ring*>          children;
    std::vector<uint64_t>       wayIds;


private:
//    std::vector<OsmGeoPosition> vertices;
    geos::geom::Polygon      *geosPolygon;
    double area;

public:
static std::vector<geos::geom::Polygon*> createSimplePolygons(const std::vector<OsmGeoPosition> &vertices, uint64_t relId);

static void deleteRecursive(Ring* ring);

/* takes a polygon that potentially has holes, and converts it to a vector of polygon rings */
static std::vector<geos::geom::Polygon*> createRings(const geos::geom::Polygon *poly, uint64_t relId);

static std::vector<Ring*> merge( const Ring* ring1, const Ring* ring2, 
                          const geos::geom::IntersectionMatrix *mat, uint64_t relId);

/* flattens an arbitrarily deep hierarchy of nested outer/inner rings
 * to a hierarchy of depth at most 2, i.e. to a list of outer rings where each outer ring
 * can contain an arbitrary number of inner rings (but contain no further nesting).
 *
 * This method does no geometric processing. It simply moves anything that is the child of an
 * inner ring to the top-level of outer rings. The input ring hierarchy is assumed to be 
 * topologically correct and overlap-free.
 */
static void flattenHierarchyToPolygons(std::vector<Ring*> &roots);


public:
/* the geometric factory has to have a life time at least as long as
   any objects it creates. So instead of creating one on-demand, we 
   store it as a singleton class variable */
static geos::geom::GeometryFactory factory;  

};

#endif

