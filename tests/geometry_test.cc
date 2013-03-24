
#include "../geometric_types.h"
#include "../vertexchain.h"
//#include "quadtree.h"
//#include "simplifypolygon.h"
//#include <stdlib.h>
//#include <assert.h>

//#include <fstream>
#include <iostream>

//#include <boost/foreach.hpp>
//#include "fraction.h"


class QuadTreeNode //copied from quadtree.cc (is normally not exposed)
{
public:
    QuadTreeNode(AABoundingBox _box);
    ~QuadTreeNode();
    void insertIntoHierarchy( LineSegment edge1, list<LineSegment> &createdSegments, int depth);
    void printHierarchy(int depth = 0);
    void exportSegments(list<LineSegment> &segments_out);
public:
    bool addToQuadrant(LineSegment seg);    
    bool intersectedByRecursive( LineSegment edge1, list<LineSegment> &createdSegments, int depth);
    int  coversQuadrants( LineSegment edge, bool &tl, bool &tr, bool &bl, bool &br) const;
    void subdivide();
    static const unsigned int SUBDIVISION_THRESHOLD = 20; //determined experimentally
    
    QuadTreeNode *tl, *tr, *bl, *br;
    list<LineSegment> segments;
    AABoundingBox     box;
    BigInt            mid_x, mid_y;
};


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

bool testQuadTreeNodeVsLine(const QuadTreeNode &node, const LineSegment &line, bool tl, bool tr, bool bl, bool br)
{
    bool in_tl, in_tr, in_bl, in_br;
    
    node.coversQuadrants( line, in_tl, in_tr, in_bl, in_br);
    
    return tl == in_tl && tr == in_tr && bl == in_bl && br == in_br;
}

void testVertexChain()
{
    VertexChain c;
    c.append(Vertex( 349935555, -852916288));
    c.append(Vertex( 349932039, -852915859));
    c.append(Vertex( 349935555, -852916288));
    c.append(Vertex( 349939642, -852917790));
    c.append(Vertex( 349939818, -852914518));
    c.append(Vertex( 349942059, -852914786));
    c.append(Vertex( 349941532, -852918488));
    c.append(Vertex( 349939642, -852917790));
    c.append(Vertex( 349935555, -852916288));
    c.isClockwise();

    c = VertexChain();
   
    c.append(Vertex(350501146,-852914867));
    c.append(Vertex(350502786,-852913921));
    c.append(Vertex(350500898,-852908663));
    c.append(Vertex(350501453,-852908376));
    c.append(Vertex(350501073,-852907376));
    c.append(Vertex(350501666,-852907054));
    c.append(Vertex(350502018,-852908073));
    c.append(Vertex(350502529,-852907706));
    c.append(Vertex(350506064,-852917147));
    c.append(Vertex(350503517,-852918542));
    c.append(Vertex(350503379,-852918105));
    c.append(Vertex(350502934,-852918351));
    c.append(Vertex(350502457,-852918614));
    c.append(Vertex(350502534,-852918571));
    c.append(Vertex(350503379,-852918105));
    c.append(Vertex(350502934,-852918351));
    c.append(Vertex(350502534,-852918571));
    c.append(Vertex(350502384,-852918740));
    c.append(Vertex(350503379,-852918105));
    c.append(Vertex(350503517,-852918542));
    c.append(Vertex(350506064,-852917147));
    c.append(Vertex(350502529,-852907706));
    c.append(Vertex(350502018,-852908073));
    c.append(Vertex(350501607,-852906874));
    c.append(Vertex(350501073,-852907376));
    c.append(Vertex(350501453,-852908376));
    c.append(Vertex(350500898,-852908663));
    c.append(Vertex(350502786,-852913921));
    c.append(Vertex(350501146,-852914867));
    c.isClockwise();
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
    //TEST( (max-min).squaredLength() == (uint64_t)16200000000000000000ull);
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
    
    //TEST( LineSegment( Vertex(0,0), Vertex(2,0)).overlapsWith( LineSegment(Vertex(1,1), Vertex(1,0)))); 
    //TEST( LineSegment( Vertex(0,0), Vertex(2,0)).overlapsWith( LineSegment(Vertex(1,0), Vertex(1,1)))); 

    {
        Vertex A(-298695466,310533612);
        Vertex B(-298811012,310385291);
        
        //Vertex X(-298814597,310397197);
        Vertex Y(-298805191,310393248);
        Vertex Z(-298809204,310384402);

        //TEST( LineSegment(A,B).intersects(LineSegment(X,Y)) );
        TEST( LineSegment(A,B).intersects(LineSegment(Y,Z)) );
        TEST( LineSegment(A,B).intersects(LineSegment(Z,Y)) );
        TEST( LineSegment(B,A).intersects(LineSegment(Y,Z)) );
        TEST( LineSegment(B,A).intersects(LineSegment(Z,Y)) );
    }

    {
        Vertex A(0,0);
        Vertex B(0,4);
        Vertex C(4,4);
        Vertex D(4,0);
        AABoundingBox box(A);
        box += B; box += C; box+= D;
        QuadTreeNode node(box);
        
        TEST( testQuadTreeNodeVsLine( node, LineSegment( Vertex(2,2), Vertex(10,10)), true, true, true, true   ));
        TEST( testQuadTreeNodeVsLine( node, LineSegment( Vertex(3,3), Vertex(10,10)), false, false, false, true   ));
        TEST( testQuadTreeNodeVsLine( node, LineSegment( Vertex(0,2), Vertex(2,4)), true, false, true, true   ));
        
        
    }
    
    testVertexChain();
}






