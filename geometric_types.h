
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
    uint64_t squaredDistanceTo(const Vertex other) const 
    { return (uint64_t)((int64_t)x - other.x)*(x-other.x) + ((int64_t)y-other.y)*(y-other.y); }
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
    void append(const Vertex& node) {m_vertices.push_back(node);}
    
    void reverse() { m_vertices.reverse();}
    void append(const PolygonSegment &other, bool exactMatch = true)
    //PolygonSegment operator+(const PolygonSegment &other, bool exactMatch = true)
    {
        if (exactMatch) 
        {
            assert(back() == other.front());
            m_vertices.insert( m_vertices.end(), ++other.m_vertices.begin(), other.m_vertices.end());
        } else
        {
            cout << "concatenating line segments that are " 
                 << sqrt( back().squaredDistanceTo(other.front()))/100.0 << "m apart"<< endl;
            m_vertices.insert( m_vertices.end(),   other.m_vertices.begin(), other.m_vertices.end());
        }
        
        //std::cout << "Merged: " << res
        //return res;
    }
//    friend std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);
private:
    std::list<Vertex> m_vertices;
};

std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
