
#ifndef GEOMETRIC_TYPES_H
#define GEOMETRIC_TYPES_H

#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <iostream>
#include <ostream>
#include <list>
using namespace std;


struct Vertex
{
    Vertex() :x(0), y(0) {}
    Vertex(int32_t v_x, int32_t v_y): x(v_x), y(v_y) {}
    uint64_t squaredDistanceTo(const Vertex other) const;
    double   distanceToLine(const Vertex A, const Vertex B) const;
    
    int32_t x, y;
    bool operator==(const Vertex other) const { return x==other.x && y == other.y;}   //no need for references, "Vertex" is small
    bool operator!=(const Vertex other) const { return x!=other.x || y != other.y;}
    bool operator< (const Vertex other) const { return (x< other.x) || ((x == other.x) && (y < other.y));}
};

std::ostream& operator <<(std::ostream& os, const Vertex v);

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

std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
