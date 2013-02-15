
#ifndef VERTEX_CHAIN_H
#define VERTEX_CHAIN_H

#include "geometric_types.h"

class PolygonSegment
{
public:
    PolygonSegment() {};
    PolygonSegment(const PolygonSegment & other): m_vertices(other.m_vertices) { }
    PolygonSegment( const list<OSMVertex> &vertices);

    PolygonSegment(const int32_t *vertices, int64_t num_vertices);
    
    const Vertex& front() const;
    const Vertex& back()  const;
    const list<Vertex>& vertices() const;
    
    void reverse();
    void append(const Vertex& node);
    void append(list<Vertex>::const_iterator begin,  list<Vertex>::const_iterator end );
    void append(const PolygonSegment &other, bool shareEndpoint);
    
    
    void canonicalize();    //remove successive identical and colinear vertices
    bool isClockwise();
    //bool isSimple() const; // FIXME: is O(n²), will be too slow for many applications
    
    /** semantics: a split line of 'clip_y' means that everything above *and including* 'clip_y' belongs to the
        upper part, everything else to the lower part    */
    void clipSecondComponent( BigInt clip_y, list<PolygonSegment> &top_out, list<PolygonSegment> &bottom_out);
    void clipFirstComponent(  BigInt clip_x, list<PolygonSegment> &left_out, list<PolygonSegment> &right_out);

    /** @returns: 'true' if the resulting polygon is a proper one, 'false' if it should be discarded completely. 
        In the latter case the state of the polygon is undefined. */
    bool simplifyArea(double allowedDeviation);
    void simplifyStroke(double allowedDeviation);
private:
    void simplifySection(list<Vertex>::iterator segment_first, list<Vertex>::iterator segment_last, uint64_t allowedDeviation);
    std::list<Vertex> m_vertices;
};

std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg);

#endif
