
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

    void serialize(FILE* fOut, bool reverseVertexOrder) const;
    uint64_t getSerializedSize() const;
    const geos::geom::Polygon* getPolygon() { return this->geosPolygon; }
    
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

/* takes a polygon that potentially has holes, and converts it to a vector of polygon rings */
static std::vector<geos::geom::Polygon*> createRings(const geos::geom::Polygon *poly, uint64_t relId);

public:
/* the geometric factory has to have a life time at least as long as
   any objects it creates. So instead of created one on-demand, we 
   store it as a singleton class variable */
static geos::geom::GeometryFactory factory;  

};

#endif

