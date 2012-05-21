
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
    Vertex(int32_t v_x, int32_t v_y): x(v_x), y(v_y) {}
    uint64_t distanceFrom(const Vertex other) const { return (uint64_t)((int64_t)x - other.x)*(x-other.x) + ((int64_t)y-other.y)*(y-other.y); }
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
    PolygonSegment operator+(const PolygonSegment &other)
    {
        assert(back() == other.front());
        PolygonSegment res = *this;
        res.m_vertices.insert( res.m_vertices.end(), ++other.m_vertices.begin(), other.m_vertices.end());
        //std::cout << "Merged: " << res
        return res;
    }
//    friend std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);
private:
    std::list<Vertex> m_vertices;
};

std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
