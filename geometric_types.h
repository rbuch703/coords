
#ifndef GEOMETRIC_TYPES_H
#define GEOMETRIC_TYPES_H

#include <stdint.h>

#include <ostream>
#include <list>

#include <set>
#include "osm_types.h"

#include "config.h"


using namespace std;

struct Vertex
{
    Vertex();
    Vertex(BigInt v_x, BigInt v_y);
    //uint64_t squaredDistanceTo(const Vertex other) const;
    BigInt squaredLength() const;
    double squaredDistanceToLine(const Vertex &A, const Vertex &B) const;
    //double   distanceToLine(const Vertex &A, const Vertex &B) const;

    /** returns the product of the signed distance to the line AB and the length of the line AB, |AB|.
      * the result is guaranteed to be exact, and is zero iff. The point lies *on* the line*/
    BigInt pseudoDistanceToLine(const Vertex &A, const Vertex &B) const;
    bool    operator==(const Vertex &other) const;
    bool    operator!=(const Vertex &other) const;
    bool    operator< (const Vertex &other) const;
    bool    operator> (const Vertex &other) const;
    Vertex  operator+(const Vertex &a) const;
    Vertex  operator-(const Vertex &a) const;
public:    
    /** just a safety precaution (wouldn't need to be BigInt, could just be int32_t): 
      * having these as BigInt ensures that all operations on them will also be performed as BigInts)*/
    BigInt x, y;    
    
};

/** semantics:  a line segment starts *at* its vertex 'start', and ends *at* its vertex 'end',
                i.e. 'start' and 'end' are part of the segment.
                By this definition, adjacent line segments making up a simple closed polygon *will* intersect*/
struct LineSegment
{
    LineSegment():start(Vertex(0,0)), end(Vertex(0,0)) {}
    LineSegment( const Vertex v_start, const Vertex v_end/*, int32_t v_tag1, int32_t v_tag2*/);
    LineSegment( BigInt start_x, BigInt start_y, BigInt end_x, BigInt end_y/*, int32_t v_tag1, int32_t v_tag2*/);

    bool isColinearWith(const Vertex v) const;
    bool isParallelTo( const LineSegment &other) const;
    bool intersects( const LineSegment &other) const;
    bool overlapsWith(const LineSegment &other) const;
    //returns the intersection of the two line segments, with each coordinate rounded *up* to the next integer
    Vertex getRoundedIntersection(const LineSegment &other) const;
    
    BigFraction getCoefficient(const Vertex v) const;
    double getIntersectionCoefficient( const LineSegment &other) const;
    //void getIntersection(LineSegment &other, BigFraction &out_x, BigFraction &out_y) const;
    void getIntersectionCoefficient( const LineSegment &other, BigInt &out_num, BigInt &out_denom) const;
    
    explicit operator bool() const;
    
    //These are purely logical comparison operators used in conjunction with container types.
    //They have no intended geometric meaning
    bool operator< (const LineSegment &other) const;
    bool operator==(const LineSegment &other) const;
    bool operator!=(const LineSegment &other) const;
    Vertex start, end;
};

inline double asDouble(int64_t a) { return a;}

inline std::ostream& operator <<(std::ostream& os, const LineSegment &edge)
{
    os << "(" << asDouble(edge.start.x) << ", " << asDouble(edge.start.y) << ") - (";
    os        << asDouble(edge.end.x)   << ", " << asDouble(edge.end.y) << ")";
    //os << edge.start << " - " << edge.end;
    return os;
}


struct AABoundingBox
{
    AABoundingBox(const Vertex v);
    AABoundingBox(BigInt t, BigInt l, BigInt b, BigInt r);

    AABoundingBox & operator+=(const Vertex v);
       
    AABoundingBox getOverlap(const AABoundingBox &other) const;
    bool overlapsWith(const AABoundingBox &other) const;
    
    BigInt width() const;
    BigInt height() const;

    BigInt top, left, bottom, right;
};



std::ostream& operator <<(std::ostream& os, const Vertex v);

bool resolveOverlap(LineSegment &A, LineSegment &B);
map<LineSegment, list<LineSegment>> findIntersectionsBruteForce(const list<LineSegment> &segments);
map<Vertex,set<Vertex> > getConnectivityGraph(const list<LineSegment> &segments );
bool intersectionsOnlyShareEndpoint(const list<LineSegment> &segments);
void moveIntersectionsToIntegerCoordinates(list<LineSegment> &segments);
void moveIntersectionsToIntegerCoordinates2(list<LineSegment> &segments);

#endif
