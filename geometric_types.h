
#ifndef GEOMETRIC_TYPES_H
#define GEOMETRIC_TYPES_H

#include <stdint.h>

#include <assert.h>

#include <ostream>
#include <list>

#include <gmpxx.h>

using namespace std;


struct Vertex
{
    Vertex();
    Vertex(mpq_class v_x, mpq_class v_y);
    //uint64_t squaredDistanceTo(const Vertex other) const;
    mpq_class squaredLength() const;
    mpq_class squaredDistanceToLine(const Vertex &A, const Vertex &B) const;
    //double   distanceToLine(const Vertex &A, const Vertex &B) const;

    /** returns the product of the signed distance to the line AB and the length of the line AB, |AB|.
      * the result is guaranteed to be exact, and is zero iff. The point lies on the line*/
    mpq_class pseudoDistanceToLine(const Vertex &A, const Vertex &B) const;
    bool    operator==(const Vertex &other) const;
    bool    operator!=(const Vertex &other) const;
    bool    operator< (const Vertex &other) const;
    Vertex  operator+(const Vertex &a) const;
    Vertex  operator-(const Vertex &a) const;
public:    
    mpq_class x, y;
    
};

Vertex operator*(const mpq_class &a, const Vertex &b);


struct AABoundingBox
{
    AABoundingBox(const Vertex v);
    AABoundingBox(mpq_class t, mpq_class l, mpq_class b, mpq_class r);

    AABoundingBox & operator+=(const Vertex v);
       
    AABoundingBox getOverlap(const AABoundingBox &other) const;
    bool overlapsWith(const AABoundingBox &other) const;
    
    mpq_class width() const;
    mpq_class height() const;

    mpq_class top, left, bottom, right;
};


/** semantics:  a line segment starts *at* its vertex 'start', and ends *right before* its vertex end,
                i.e. 'start' is part of the segment, while 'end' is not.
                By this definition, adjacent line segments making up a simple closed polygon do not intersect*/
struct LineSegment
{
    LineSegment( const Vertex v_start, const Vertex v_end/*, int32_t v_tag1, int32_t v_tag2*/);
    LineSegment( mpq_class start_x, mpq_class start_y, mpq_class end_x, mpq_class end_y/*, int32_t v_tag1, int32_t v_tag2*/);

    bool isColinearWith(const Vertex v) const;                                                  
    bool parallelTo( const LineSegment &other) const;
    bool intersects( const LineSegment &other) const;
    mpq_class getIntersectionCoefficient( const LineSegment &other) const;
//    void   getIntersectionCoefficient( const LineSegment &other, int64_t &out_num, int64_t &out_denom) const;
    
    Vertex start, end;
};


class PolygonSegment
{
public:
    PolygonSegment() {};
    PolygonSegment(const PolygonSegment & other): m_vertices(other.m_vertices) { }
    PolygonSegment(const int32_t *vertices, int64_t num_vertices);
    
    const Vertex& front() const;
    const Vertex& back()  const;
    const list<Vertex>& vertices() const;
    
    void reverse();
    void append(const Vertex& node);
    void append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end );
    void append(const PolygonSegment &other, bool shareEndpoint);
    
    
    void canonicalize();    //remove successive identical and colinear vertices
    bool isClockwise() const;
    bool isSimple() const; // FIXME: is O(n²), will be too slow for many applications
    
    /** semantics: a split line of 'clip_y' means that everything above *and including* 'clip_y' belongs to the
        upper part, everything else to the lower part    */
    void clipSecondComponent( mpq_class clip_y, list<PolygonSegment> &top_out, list<PolygonSegment> &bottom_out) const;
    void clipFirstComponent(  mpq_class clip_x, list<PolygonSegment> &left_out, list<PolygonSegment> &right_out) const;

    /** @returns: 'true' if the resulting polygon is a proper one, 'false' if it should be discarded completely. 
        In the latter case the state of the polygon is undefined. */
    bool simplifyArea(double allowedDeviation);
    void simplifyStroke(double allowedDeviation);
private:
    void simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation);
    std::list<Vertex> m_vertices;
};

std::ostream& operator <<(std::ostream& os, const Vertex v);
std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
