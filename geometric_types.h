
#ifndef GEOMETRIC_TYPES_H
#define GEOMETRIC_TYPES_H

#include <stdint.h>

#include <assert.h>

#include <ostream>
#include <list>

using namespace std;


struct Vertex
{
    Vertex() :x(0), y(0) {}
    Vertex(int64_t v_x, int64_t v_y): x(v_x), y(v_y) { /*assert( x <= 3600000000 && x >= -3600000000 && y <= 1800000000 && y>= -1800000000);*/}
    //uint64_t squaredDistanceTo(const Vertex other) const;
    uint64_t squaredLength() const { return ((uint64_t)x*x)+((uint64_t)y*y);}
    double   distanceToLine(const Vertex A, const Vertex B) const;

    /** returns the product of the signed distance to the line AB and the length of the line AB, |AB|.
      * the result is guaranteed to be exact, and is zero iff. The point lies on the line*/
    int64_t pseudoDistanceToLine(const Vertex A, const Vertex B) const;
    bool operator==(const Vertex other) const { return x==other.x && y == other.y;}   //no need for references, "Vertex" is small
    bool operator!=(const Vertex other) const { return x!=other.x || y != other.y;}
    bool operator< (const Vertex other) const { return (x< other.x) || ((x == other.x) && (y < other.y));}
    
    Vertex operator+(const Vertex a) const { return Vertex(x+a.x, y+a.y);}
    Vertex operator-(const Vertex a) const { return Vertex(x-a.x, y-a.y);}
public:    
    int64_t x, y;
    
};


struct AABoundingBox
{
    AABoundingBox(const Vertex v) { left = right= v.x; top=bottom=v.y;}
    AABoundingBox(int64_t t, int64_t l, int64_t b, int64_t r): top(t), left(l), bottom(b), right(r) { }
    AABoundingBox & operator+=(const Vertex v) {
        if (v.x < left) left = v.x;
        if (v.x > right) right = v.x;
        if (v.y < top) top = v.y;
        if (v.y > bottom) bottom = v.y;
        return *this;
    }
       
    AABoundingBox getOverlap(const AABoundingBox &other) const
    {
        assert ( isNormalized() && other.isNormalized());
        int64_t new_left = max( left, other.left);
        int64_t new_right= min( right, other.right);
        assert(new_left <= new_right);
        
        int64_t new_top  = max( top, other.top);
        int64_t new_bottom=min( bottom, other.bottom);
        assert(new_top <= new_bottom);
        
        AABoundingBox box(Vertex(new_left, new_top));
        box+= Vertex(new_right, new_bottom);
        return box;
    }
    
    bool overlapsWith(const AABoundingBox &other) const
    {  
        assert ( isNormalized() && other.isNormalized());
        return (max( left, other.left) <= min( right, other.right)) && 
               (max( top, other.top)   <= min( bottom, other.bottom));    
    }
    
    
    int64_t width() const { return right - left;}
    int64_t height() const{ return bottom - top;}
public:    
    int64_t top, left, bottom, right;
    
private:
    bool isNormalized() const { return right >= left && bottom >= top;}
};


/** semantics:  a line segment starts *at* its vertex 'start', and ends *right before* its vertex end,
                i.e. 'start' is part of the segment, while 'end' is not.
                With this definition, adjacent line segments making up a simple closed polygon do not intersect*/
struct LineSegment
{
    LineSegment( const Vertex v_start, const Vertex v_end, int32_t v_tag1, int32_t v_tag2);
    LineSegment( int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y, int32_t v_tag1, int32_t v_tag2);

    bool parallelTo( const LineSegment &other) const { return (end.x- start.x)*(other.end.y - other.start.y) ==
                                                              (end.y- start.y)*(other.end.x - other.start.x);}
    bool contains(const Vertex v) const;                                                  
    bool intersects( const LineSegment &other) const;
    double getIntersectionCoefficient( const LineSegment &other) const;
    
    Vertex start, end;
    int32_t tag1, tag2;
};



typedef int64_t (*VertexCoordinate)(const Vertex &v);

class PolygonSegment
{
public:
    PolygonSegment() {};
    PolygonSegment(const PolygonSegment & other): m_vertices(other.m_vertices) { }
    //PolygonSegment& operator=(const PolygonSegment & other);
    
    const Vertex& front() const { return m_vertices.front();}
    const Vertex& back()  const { return m_vertices.back();}
    const list<Vertex>& vertices() const { return m_vertices;}
    
    void reverse() { m_vertices.reverse();}
    void append(const Vertex& node) {m_vertices.push_back(node);}
    void append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end ) 
                {m_vertices.insert(m_vertices.end(),begin, end);}
    void append(const PolygonSegment &other, bool shareEndpoint);
    
    /** 
        semantics: a split line of 'clip_y' means that everything above *and including* 'clip_y' belongs to the
        upper part, everything else to the lower part
    */
    //template<VertexCoordinate significantCoordinate, VertexCoordinate otherCoordinate> void clipComponent(int32_t clip,
    //    list<PolygonSegment> &top_out, list<PolygonSegment> &bottom_out) const;
    
    
    void clipSecondComponent( int32_t clip_y, list<PolygonSegment> &top_out, list<PolygonSegment> &bottom_out) const;
    void clipFirstComponent(  int32_t clip_x, list<PolygonSegment> &left_out, list<PolygonSegment> &right_out) const;

    /** @returns: 'true' if the resulting polygon is a proper one, 'false' it should be discarded completely. 
        In the latter case, the state of the polygon is undefined. */
    bool simplifyArea(double allowedDeviation);
    void simplifyStroke(double allowedDeviation);
    
private:
    void simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation);
    
//    friend std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);
private:
    std::list<Vertex> m_vertices;
};

void createSimplePolygons(const list<Vertex> in, const list<LineSegment> intersections, list<PolygonSegment> &out);
//void createSimplePolygons(list<Vertex> in, list<PolygonSegment> &out);
/** each uint32_t is the start vertex of a line
    'start' vertex here means the vertex with the lower index of the two line end points (the other vertex of
    each line is always at 'start vertex'+1 */
//typedef pair<uint32_t, uint32_t> Intersection;

void findIntersections(const list<Vertex> &path, list<LineSegment> &intersections_out);
/*
class Polygon
{
protected:
    void canonicalize();
private:
private:
    uint32_t m_num_vertices;
    Vertex* m_vertices;
};
*/
std::ostream& operator <<(std::ostream& os, const Vertex v);
std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
