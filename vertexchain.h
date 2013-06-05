
#ifndef VERTEX_CHAIN_H
#define VERTEX_CHAIN_H

#include "geometric_types.h"
#include <vector>

class VertexChain
{
public:
    VertexChain() {};
    VertexChain(const VertexChain &other): m_vertices(other.m_vertices) { }
    VertexChain(const vector<Vertex> &vertices): m_vertices(vertices) { }
    VertexChain(const list<Vertex> &vertices): m_vertices(vertices.begin(), vertices.end()) { }
    VertexChain(const list<OSMVertex> &vertices);

    VertexChain(const int32_t *vertices, int64_t num_vertices);
    
    const Vertex& front() const;
    const Vertex& back()  const;
    const vector<Vertex>& data() const;
    vector<Vertex>& internal_data() { return m_vertices;}
    
    void reverse();
    void append(const Vertex& node);
    void append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end );
    void append(const VertexChain &other, bool shareEndpoint);
    void mirrorX();
    
    AABoundingBox getBoundingBox() const;    
    void canonicalize();    //remove successive identical and colinear vertices
    
    bool isCanonicalPolygon();
    bool isClockwise();
    
    uint64_t size() const { return m_vertices.size(); }
    void clear() { m_vertices.clear(); }
    
    /** semantics: a split line of 'clip_y' means that everything above *and including* 'clip_y' belongs to the
        upper part, everything else to the lower part    */
    void clipSecondComponent( BigInt clip_y, bool closePolygons, list<VertexChain> &top_out, list<VertexChain> &bottom_out);
    void clipFirstComponent(  BigInt clip_x, bool closePolygons, list<VertexChain> &left_out, list<VertexChain> &right_out);

    /** @returns: 'true' if the resulting polygon is a proper one, 'false' if it should be discarded completely. 
        In the latter case the state of the polygon is undefined. */
    bool simplifyArea(double allowedDeviation);
    bool simplifyStroke(double allowedDeviation);
    //list<VertexChain> toSimplePolygon();
private:
    void canonicalizeLineSegment();
    void canonicalizePolygon();

    //void simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation);
    std::vector<Vertex> m_vertices;
};

std::ostream& operator <<(std::ostream& os, const VertexChain &seg);

#endif
