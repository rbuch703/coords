
#include "geometric_types.h"
#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>

#include <boost/foreach.hpp>
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[22;37m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}


bool intersects (const ActiveEdge &edge, BigInt xPos)
{
    assert (edge.left < edge.right);
    
    return edge.left.x <= xPos && edge.right.x > xPos;
}

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

    for (int i = 0; i < 100000; i++)
    {
        BigInt a( getRandom());
        
        uint32_t b = rand() % 0xFFFFFFFF;

        BigInt res = a/b;
    }
    
/*
    static const int32_t NUM_EDGES = 100000;
    list<ActiveEdge> edges;
    LineArrangement l;
    for (int i = 0; i < NUM_EDGES; i++)
    {
        Vertex v1(rand() % 65536, rand() % 65536);
        Vertex v2(rand() % 65536, rand() % 65536);

        edges.push_back( ActiveEdge(v1 < v2 ? v1:v2 , v1<v2? v2:v1, true, NULL) );
        //std::cout << edges[i] << endl;
    }

    BigInt xPos = rand() % 65536;

    std::cout << "=======================" << xPos << "======================="<< std::endl;
    BOOST_FOREACH( const ActiveEdge &edge, edges)
    {
        if (intersects(edge, xPos)) 
            l.addEdge(edge, xPos);
    }        
    std::cout << "=======================" << xPos << "======================="<< std::endl;

    std::cout << l.size() << std::endl;

    BigInt prev_pos = 0;
    LineSegment s(xPos, 0, xPos, 65536);
    for (LineArrangement::const_iterator it = l.begin(); it != l.end(); it++)
    {
        LineSegment e( it->left, it->right);
        assert( e.intersects(s));
        double alpha = e.getIntersectionCoefficient(s);
        //std::cout << alpha.get_d() << endl;
        Vertex intersect( (int32_t)(it->left.x.toDouble() + alpha* (it->right.x - it->left.x).toDouble() ),
                          (int32_t)(it->left.y.toDouble() + alpha* (it->right.y - it->left.y).toDouble() ) );
        //std::cout << intersect << "<=" << prev_pos << std::endl;

        assert( intersect.y >= prev_pos);
        prev_pos = intersect.y;
        //std::cout << (*it)  << " [" << intersect << "] "<< endl;
    }
    */

#if 0
    Vertex a(1,0);
    Vertex b(0,1);
    
    TEST( (a-b).squaredLength() == 2);
    TEST( (b-a).squaredLength() == 2);
    //overflow tests for OSM vertices; note that for the longitude (y-coordinate), the last-but-one bit is always zero
    Vertex min(-1800000000, -900000000);
    Vertex max( 1800000000,  900000000);
    TEST( (max-min).squaredLength() == 16200000000000000000ull);
    /*
    a= Vertex(0,0);
    b= Vertex(1000,1000);*/
    
    LineSegment seg(min, max, -1, -1);
    TEST( seg.contains(Vertex(0,0) ));
    TEST(!seg.contains(Vertex(1,0)) );
    TEST( seg.contains(Vertex(2, 1) ));
    TEST( seg.contains(Vertex(1000, 500) ));
    TEST(!seg.contains(Vertex(999, 500) ));
    TEST( seg.contains( max - Vertex(2,1)) );
    TEST( seg.contains(min) );
    TEST(!seg.contains(max) );
    
    seg = LineSegment( Vertex(900000000, -900000000), Vertex(-900000000, 900000000), -1, -1);
    TEST( seg.contains(seg.start));
    TEST(!seg.contains(seg.end));
    TEST( seg.contains(Vertex( 0, 0)));
    TEST( seg.contains(Vertex( 1,-1)));
    TEST( seg.contains(Vertex(-1, 1)));
    TEST(!seg.contains(Vertex(-1,-1)));
    TEST(!seg.contains(Vertex( 1, 1)));
    TEST( seg.parallelTo( LineSegment( a-b, b-a, -1, -1)));
    TEST( seg.parallelTo( LineSegment( b-a, a-b, -1, -1)));
    TEST( seg.parallelTo( LineSegment( Vertex(0, 1), Vertex(1, 0), -1, -1)));
    TEST(!seg.parallelTo( LineSegment( Vertex(0, 0), Vertex(1, 1), -1, -1)));
    TEST(!seg.parallelTo( LineSegment( Vertex(-1, -1), Vertex(1, 1), -1, -1)));
    
    seg = LineSegment( Vertex(0,0), Vertex(2,0), -1, -1);
    TEST( Vertex(0, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(-1, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(2, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(1, 1).pseudoDistanceToLine( seg.start, seg.end) == -2);
    TEST( Vertex(1,-1).pseudoDistanceToLine( seg.start, seg.end) ==  2);
    
    Vertex A(0,0);
    Vertex B(0,2);
    Vertex C(2,0);
    Vertex D(2,2);
    TEST( LineSegment(A, D, -1, -1).intersects(LineSegment(B, C, -1, -1)));
    TEST( LineSegment(D, A, -1, -1).intersects(LineSegment(C, B, -1, -1)));
    TEST(!LineSegment(A, B, -1, -1).intersects(LineSegment(C, D, -1, -1)));
    TEST(!LineSegment(A, C, -1, -1).intersects(LineSegment(D, B, -1, -1)));

    TEST( LineSegment(A, B, -1, -1).intersects(LineSegment(A, D, -1, -1)));
    TEST(!LineSegment(B, A, -1, -1).intersects(LineSegment(A, D, -1, -1)));
    TEST(!LineSegment(C, D, -1, -1).intersects(LineSegment(B, D, -1, -1))); //only share an endpoint, which is not part of the line
    PolygonSegment p;
    p.append(Vertex(0,1));
    p.append(Vertex(1,0));
    p.append(Vertex(2,0));
    p.append(Vertex(2,4));
    p.append(Vertex(1,4));
    p.append(Vertex(0,3));
    p.append(Vertex(1,2));
    p.append(Vertex(0,1));
#endif    
   
}

