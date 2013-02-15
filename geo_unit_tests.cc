
#include "geometric_types.h"
#include "vertexchain.h"
//#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>

#include <boost/foreach.hpp>
#include "fraction.h"

typedef Fraction<int128_t> BigFrac;
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[1;30m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

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

void testOverlapResolution( LineSegment A, LineSegment B, LineSegment res_A, LineSegment res_B)
{
    resolveOverlap(A, B);
    TEST( A == res_A && B == res_B);
}

int main()
{

    Vertex a(1,0);
    Vertex b(0,1);
    
    TEST( (a-b).squaredLength() == 2);
    TEST( (b-a).squaredLength() == 2);
    //overflow tests for OSM vertices; note that for the longitude (y-coordinate), the last-but-one bit is always zero
    Vertex min(-1800000000, -900000000);
    Vertex max( 1800000000,  900000000);
    TEST( (max-min).squaredLength() == (uint64_t)16200000000000000000ull);
    //a= Vertex(0,0);
    //b= Vertex(1000,1000);
    
    LineSegment seg(min, max);
/*    TEST( seg.contains(Vertex(0,0) ));
    TEST(!seg.contains(Vertex(1,0)) );
    TEST( seg.contains(Vertex(2, 1) ));
    TEST( seg.contains(Vertex(1000, 500) ));
    TEST(!seg.contains(Vertex(999, 500) ));
    TEST( seg.contains( max - Vertex(2,1)) );
    TEST( seg.contains(min) );
    TEST(!seg.contains(max) );*/
    
    seg = LineSegment( Vertex(900000000, -900000000), Vertex(-900000000, 900000000));
/*    TEST( seg.contains(seg.start));
    TEST(!seg.contains(seg.end));
    TEST( seg.contains(Vertex( 0, 0)));
    TEST( seg.contains(Vertex( 1,-1)));
    TEST( seg.contains(Vertex(-1, 1)));
    TEST(!seg.contains(Vertex(-1,-1)));
    TEST(!seg.contains(Vertex( 1, 1)));*/
    TEST( seg.isParallelTo( LineSegment( a-b, b-a)));
    TEST( seg.isParallelTo( LineSegment( b-a, a-b)));
    TEST( seg.isParallelTo( LineSegment(  0,  1, 1, 0 )));
    TEST(!seg.isParallelTo( LineSegment(  0,  0, 1, 1 )));
    TEST(!seg.isParallelTo( LineSegment( -1, -1, 1, 1 )));
    
    seg = LineSegment( Vertex(0,0), Vertex(2,0) );
    TEST( Vertex(0, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(-1, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(2, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(1, 1).pseudoDistanceToLine( seg.start, seg.end) == -2);
    TEST( Vertex(1,-1).pseudoDistanceToLine( seg.start, seg.end) ==  2);
    
    Vertex A(0,0);
    Vertex B(0,2);
    Vertex C(2,0);
    Vertex D(2,2);
    TEST( LineSegment(A, D).intersects(LineSegment(B, C)));
    TEST( LineSegment(D, A).intersects(LineSegment(C, B)));
    TEST(!LineSegment(A, B).intersects(LineSegment(C, D)));
    TEST(!LineSegment(A, C).intersects(LineSegment(D, B)));

    TEST( LineSegment(A, B).intersects(LineSegment(A, D)));
    TEST( LineSegment(B, A).intersects(LineSegment(A, D)));
    TEST( LineSegment(C, D).intersects(LineSegment(B, D))); 
    
    {
        LineSegment A(Vertex(0,0), Vertex(2,2));
        LineSegment B(Vertex(0,0), Vertex(3,3));
        LineSegment C(Vertex(-1,-1), Vertex(2,2));
        testOverlapResolution(A, B, LineSegment(), B);
        testOverlapResolution(A, C, LineSegment(), C);
        testOverlapResolution(B, C, B, LineSegment(Vertex(-1,-1), Vertex(0,0)));
        testOverlapResolution(B, A, B, LineSegment());
        testOverlapResolution(C, A, C, LineSegment());
        testOverlapResolution(C, B, C, LineSegment(Vertex(2,2), Vertex(3,3)));
        testOverlapResolution(A, A, A, LineSegment());
        testOverlapResolution(A, LineSegment(Vertex(3,3), Vertex(1,1)), A, LineSegment(Vertex(3,3), Vertex(2,2)));
        
    }

    VertexChain p;
    p.append(Vertex(0,1));
    p.append(Vertex(1,0));
    p.append(Vertex(2,0));
    p.append(Vertex(2,4));
    p.append(Vertex(1,4));
    p.append(Vertex(0,3));
    p.append(Vertex(3,2));  //was 1,2
    p.append(Vertex(0,1));
    
    //int numVertices = p.vertices().size() - (p.front() == p.back() ? 1 : 0);
    list<LineSegment> segs;
    const list<Vertex> &verts = p.vertices();
    
    list<Vertex>::const_iterator it2 = verts.begin();
    for (list<Vertex>::const_iterator it = it2++; it2 != verts.end(); it++, it2++)
    {
        segs.push_back( LineSegment(*it, *it2) );
    }
    
    moveIntersectionsToIntegerCoordinates(segs);
    std::cout << "== finished preparations == " << std::endl;
    BOOST_FOREACH( LineSegment seg, segs)
        std::cout << "# " << seg << std::endl;
    
    TEST( intersectionsOnlyShareEndpoint(segs) );
    map<LineSegment, list<LineSegment>> intersections = findIntersectionsBruteForce(segs);
    map<Vertex,set<Vertex> > graph = getConnectivityGraph(segs);

    int numEdges = 0;
    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
        for(set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
        {
            std::cout << "\t" << it->first << ", " << *it2 << std::endl;
        }
        numEdges += it->second.size();
    }
    std::cout << "Connectivity graph consists of " << graph.size() << " vertices and " << numEdges << " edges." << std::endl;
    //std::cout << "found " << numIntersections << " intersections on " << intersections.size() 
    //          << " line segments from " << numVertices << " vertices" << std::endl;
}




