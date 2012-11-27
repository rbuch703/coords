
#include "geometric_types.h"
#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[22;37m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

int main()
{
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
#endif    
    PolygonSegment p;
    p.append(Vertex(0,1));
    p.append(Vertex(1,0));
    p.append(Vertex(2,0));
    p.append(Vertex(2,4));
    p.append(Vertex(1,4));
    p.append(Vertex(0,3));
    p.append(Vertex(1,2));
    p.append(Vertex(0,1));

                

    list<PolygonSegment> segs;
    
    ActiveEdge a( Vertex(0,0), Vertex(2,2), true, NULL);
    ActiveEdge b( Vertex(0,2), Vertex(2,0), true, NULL);
    
    LineArrangement l;
    int32_t yPos = 0;
    l.addEdge(a, yPos);
    l.addEdge(b, yPos);
    l.addEdge(ActiveEdge( Vertex(0,1), Vertex(2,1), true, NULL) , yPos);
    for (LineArrangement::const_iterator it = l.begin(); it != l.end(); it++)
    {
        std::cout << *it << endl;
    }
    //simplifyPolygon(p, segs);    
    //cout << ints.size()/2 << endl;

}

