
#ifndef GEOMETRIC_TYPES_H
#define GEOMETRIC_TYPES_H

#include <stdint.h>

#include <ostream>
#include <list>
#include <vector>
#include <set>

#include "osm_types.h"

#include "config.h"


using namespace std;

class QuadTreeNode;
//enum EventType;

/* there may be hundreds of millions of vertices in memory at a given time, so
   memory consumption due to member alignment is a big concern. */
struct __attribute__ ((aligned (4))) Vertex
{
    Vertex();
    Vertex(BigInt v_x, BigInt v_y);
    inline Vertex(int32_t v_x, int32_t v_y): x(v_x), y(v_y) {};
    Vertex(bool valid); //readable way of creating an invalidated Vertex
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
    inline BigInt get_x() const { return BigInt(x);}
    inline BigInt get_y() const { return BigInt(y);}
    inline bool isZero() const { return (x == 0) && (y == 0); }
private:    
    int32_t x,y;
    /*the following friend declarations are really just a whitelist of methods/classes that 
      can guarantee that operations on int32_t values will not overflow */
    friend void dumpPolygon(string file_base, const vector<Vertex>& poly);
    friend std::ostream& operator <<(std::ostream& os, const Vertex v);
    friend struct AABoundingBox;
    friend class QuadTreeNode;
//    friend EventType classifyVertex(const vector<Vertex> &vertices, uint64_t vertex_id);
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
    bool intersects( const LineSegment &other, BigFraction &intersect_x_out, BigFraction &intersect_y_out) const;
    
    bool overlapsWith(const LineSegment &other) const;
    //returns the intersection of the two line segments, with each coordinate rounded *up* to the next integer
    Vertex getRoundedIntersection(const LineSegment &other) const;
    
    BigFraction getCoefficient(const Vertex v) const;
    //double getIntersectionCoefficient( const LineSegment &other) const;
    //void getIntersection(LineSegment &other, BigFraction &out_x, BigFraction &out_y) const;
    void getIntersectionCoefficient( const LineSegment &other, BigInt &out_num, BigInt &out_denom) const;
    
    explicit operator bool() const;
    
    //These are purely logical comparison operators used in conjunction with container types.
    //They have no intended geometric meaning
    bool operator< (const LineSegment &other) const;
    bool operator==(const LineSegment &other) const;
    bool operator!=(const LineSegment &other) const;
    
private:
    bool intersects( const LineSegment &other, BigInt &num1, BigInt &num2) const;

public:
    Vertex start, end;
};

void handleIntersection(LineSegment &seg1, LineSegment &seg2, LineSegment &seg1_aux, LineSegment &seg2_aux, 
                        bool &seg1_modified, bool &seg2_modified);

inline double asDouble(int64_t a) { return a;}

inline double asDouble(int128_t a) { return a.toDouble();}

#ifdef VALIDATINGBIGINT_H
inline double asDouble(ValidatingBigint a) { return a.toDouble();}
#endif

std::ostream& operator <<(std::ostream &os, const LineSegment edge);


/* BEWARE: the AABB has orientation semantics different from all other classes: 
           For an AABB "top" refers to the line with the minimum y coordinate (of the AABB), whereas
           All other classes assume 'top' to have the maximum relevant y value.
           Thus, the meaning of 'top' and 'bottom' (and by extension the meaning of'left and 'right'
           for the orientation test) are switched for AABBs
            */
struct AABoundingBox
{
    AABoundingBox(const Vertex v);
    AABoundingBox(Vertex tl_, Vertex br_);

    AABoundingBox & operator+=(const Vertex v);
       
    AABoundingBox getOverlap(const AABoundingBox &other) const;
    bool overlapsWith(const AABoundingBox &other) const;
    
    int32_t left()   const { return tl.x; }
    int32_t right()  const { return br.x; }
    int32_t top()    const { return tl.y; }
    int32_t bottom() const { return br.y; }
    BigInt width()  const;
    BigInt height() const;

    Vertex tl,br;
};

std::ostream& operator <<(std::ostream& os, const AABoundingBox v);


std::ostream& operator <<(std::ostream& os, const Vertex v);

bool resolveOverlap(LineSegment &A, LineSegment &B);
map<LineSegment, list<LineSegment>> findIntersectionsBruteForce(const list<LineSegment> &segments);
bool intersectionsOnlyShareEndpoint(const list<LineSegment> &segments);
void moveIntersectionsToIntegerCoordinates(list<LineSegment> &segments);

#endif
