
#include "../geometric_types.h"
//#include "vertexchain.h"
//#include "quadtree.h"
//#include "simplifypolygon.h"
//#include <stdlib.h>
//#include <assert.h>

//#include <fstream>
#include <iostream>

//#include <boost/foreach.hpp>
//#include "fraction.h"

typedef Fraction<BigInt> BigFrac;
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[1;30m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

BigInt getRandom()
{
    BigInt a = 0;
    int i = rand() % 8 + 1;
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



int main(int, char** )
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
 
}




