
#include "triangulation.h"

#include "../vertexchain.h"
#include "../quadtree.h" // for toSimplePolygons()
#include <boost/foreach.hpp>

/**
    Data Structures:
        - priority queue of events (new segment, end of segment, intersection), 
            - sorted by sweep direction (first by x-coordinate, then by y-coordinate)
            - ability to remove elements in O( log(n) ) (mostly intersections that did not materialize)
            
        - list of active edges (those polygon edges that intersect the current sweep line), 
            - sorted by current y-coordinate
            - ability to swap adjacent edges in O(1) (when they intersect)
            - ability to find edges in O(log(n)) (e.g. the active edge matching the current event)
            - ability to remove edges in O(log(n)) (when they no longer intersec the sweep line)
*/

BigInt getRandom()
{
    BigInt a = 0;
    int i = 8;
    do
    {
        a = (a << 16)  | (rand() % 0xFFFF);
    } while (--i);
    
    return rand() % 2 ? a : -a;
    
}

int main()
{
    VertexChain p; 

    srand(24);
    for (int i = 0; i < 50; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

    list<VertexChain> simples = toSimplePolygons( p );
    VertexChain poly = simples.front();
    BOOST_FOREACH( VertexChain c, simples)
        if ( c.size() > poly.size())
            poly = c;
            
    std::cout << "generated simple polygon with " << poly.size() << " vertices" << std::endl;
    poly.canonicalize();
    vector<Vertex> verts = poly.data();
    assert( verts.front() == verts.back() );
    verts.pop_back();
    
    assert(verts.size() > 3 && "TODO: handle special case were polygon is already a triangle (or degenerated)");
    MonotonizeEventQueue events;
    for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);
}






