
#ifndef GEOMETRIC_TYPES_H
#define GEOMETRIC_TYPES_H

#include <stdint.h>
#include <assert.h>
#include <stdlib.h> //for llabs
#include <math.h>

#include <iostream>
#include <ostream>
#include <list>
using namespace std;

struct LineSegment;


struct Vertex
{
    Vertex() :x(0), y(0) {}
    Vertex(int64_t v_x, int64_t v_y): x(v_x), y(v_y) { assert( x <= 3600000000 && x >= -3600000000 && y <= 1800000000 && y>= -1800000000);}
    //uint64_t squaredDistanceTo(const Vertex other) const;
    uint64_t squaredLength() const { return ((uint64_t)x*x)+((uint64_t)y*y);}
    double   distanceToLine(const Vertex A, const Vertex B) const;

    /** returns the product of the signed distance to the line AB and the length of the line AB, |AB|.
      * the result is guaranteed to be exact, and is zero iff. The point lies on the line*/
    int64_t pseudoDistanceToLine(const Vertex A, const Vertex B) const;
    int64_t x, y;
    bool operator==(const Vertex other) const { return x==other.x && y == other.y;}   //no need for references, "Vertex" is small
    bool operator!=(const Vertex other) const { return x!=other.x || y != other.y;}
    bool operator< (const Vertex other) const { return (x< other.x) || ((x == other.x) && (y < other.y));}
    
    Vertex operator+(const Vertex a) const { return Vertex(x+a.x, y+a.y);}
    Vertex operator-(const Vertex a) const { return Vertex(x-a.x, y-a.y);}
};


/** semantics:  a line segment starts *at* its vertex 'start', and ends *right before* its vertex end,
                i.e. 'start' is part of the segment, while 'end' is not.
                With this definition, adjacent line segments making up a simple closed polygon do not intersect*/
struct LineSegment
{
    LineSegment( const Vertex v_start, const Vertex v_end): start(v_start), end(v_end) 
    { assert(start != end);}
    LineSegment( int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y): start(Vertex(start_x, start_y)), end(Vertex(end_x, end_y)) 
    { assert(start != end);}

    bool parallelTo( const LineSegment &other) const { return (end.x- start.x)*(other.end.y - other.start.y) ==
                                                              (end.y- start.y)*(other.end.x - other.start.x);}
    bool contains(const Vertex v) const
    {
        //test whether the vertex lies on the line (not necessarily the line segment)
        if (( (int64_t)v.x-start.x)*((int64_t)end.y - start.y) != 
            ( (int64_t)v.y-start.y)*((int64_t)end.x - start.x)) return false;
        
        uint64_t squared_seg_len = (end - start).squaredLength();
        uint64_t dist_start =      (v - start).squaredLength();
        uint64_t dist_end =        (v - end).squaredLength();
        return  dist_start <squared_seg_len && dist_end <= squared_seg_len;
    }
                       
                                                  
    bool intersects( const LineSegment &other) const
    {
        if (parallelTo(other))
        {
            // if the two parallel line segments do not lie on the same line, they cannot intersect
            if (start.pseudoDistanceToLine(other.start, other.end) != 0) return false;
            return contains(other.start) || contains(other.end) || other.contains(start) || other.contains(end );
        } else
        {
            int64_t num1 = (other.end.x-other.start.x)*(start.y-other.start.y) - (other.end.y-other.start.y)*(start.x-other.start.x);
            int64_t num2 = (      end.x-      start.x)*(start.y-other.start.y) - (      end.y-      start.y)*(start.x-other.start.x);
            int64_t denom= (other.end.y-other.start.y)*(end.x  -      start.x) - (other.end.x-other.start.x)*(end.y  -      start.y);

            assert(denom != 0); //should only be zero if the lines are parallel, but this case has already been handled above

            //for the line segments to intersect, num1/denom and num2/denom both have to be inside the range [0, 1);
            
            //the absolute of at least one coefficient would be bigger than or equal to one 
            if (llabs(num1) >=llabs(denom) || llabs(num2) >= llabs(denom)) return false; 
            
            // at least one coefficient would be negative
            if (( num1 < 0 || num2 < 0) && denom > 0) return false; 
            if (( num1 > 0 || num2 > 0) && denom < 0) return false; // coefficient would be negative
            
            return true; // not negative and not bigger than/equal to one --> segments intersect
        }
        
    }

    Vertex start, end;
};


class PolygonSegment
{
public:
    const Vertex& front() const { return m_vertices.front();}
    const Vertex& back()  const { return m_vertices.back();}
    const list<Vertex>& vertices() const { return m_vertices;}
    
    void reverse() { m_vertices.reverse();}
    void append(const Vertex& node) {m_vertices.push_back(node);}
    void append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end ) 
                {m_vertices.insert(m_vertices.end(),begin, end);}
    void append(const PolygonSegment &other, bool shareEndpoint);
    /*
    void clipHorizontally( uint32_t height, list<PolygonSegment> &top_out, list<PolygonSegment> &bottom_out)
    {
        #warning continue here
    }*/
    /** returns whether the resulting polygon is a proper one, or if it is smaller than the given threshold 
        and should be discarded completely. In the latter case, the state of the polygon is undefined. */
    bool simplifyArea(double allowedDeviation);
    void simplifyStroke(double allowedDeviation);
    
private:
    void simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation);
    
//    list<Vertex> &getVerticesDEBUG() { return m_vertices;} // TODO: debug api, remove this  
//    friend std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);
private:
    std::list<Vertex> m_vertices;
};

/** each uint32_t is the start vertex of a line
    'start' vertex here means the vertex with the lower index of the two line end points (the other vertex of
    each line is always at 'start vertex'+1 */
typedef pair<uint32_t, uint32_t> Intersection;

void findIntersections(list<Vertex> path, list<Intersection> intersections_out);

class Polygon
{
protected:
    void canonicalize();
private:
private:
    uint32_t m_num_vertices;
    Vertex* m_vertices;
};

std::ostream& operator <<(std::ostream& os, const Vertex v);
std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
