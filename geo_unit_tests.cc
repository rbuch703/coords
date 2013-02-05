
#include "geometric_types.h"
#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>

#include <boost/foreach.hpp>
#include "fraction.h"

typedef Fraction<int128_t> BigFrac;
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[22;37m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

typedef AVLTree<SimpEvent> SweepEventQueue;

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

void getIntersection(ActiveEdge &a, ActiveEdge &b, BigFraction &out_x, BigFraction &out_y) 
{
    LineSegment A(a.left, a.right);
    LineSegment B(b.left, b.right);

    assert (A.intersects(B));
    BigInt num, denom;
    BigFraction coeff(num, denom);
    
    A.getIntersectionCoefficient(B, num, denom);
    
    out_x = coeff*(A.end.x - A.start.x) + A.start.x;
    out_y = coeff*(A.end.y - A.start.y) + A.start.y;
}

bool intersect(ActiveEdge a, ActiveEdge b)
{
    return LineSegment(a.left, a.right).intersects(LineSegment(b.left, b.right));
}

void scheduleIntersectionIfExists(ActiveEdge a, ActiveEdge b, BigFraction x_pos, BigFraction y_pos, SimpEventQueue &events)
{
    if (intersect(a, b))
    {
        BigFraction x,y;
                    
        getIntersection(a, b, /*out*/x, /*out*/y);
        assert ( (x != x_pos || y != y_pos) && "not implemented");
        
        if ( x > x_pos || (x == x_pos && y > y_pos))
            events.add( INTERSECTION, a, b);
    }
}

int main()
{

    static const int32_t NUM_EDGES = 250;
    list<ActiveEdge> edges;
    for (int i = 0; i < NUM_EDGES; i++)
    {
        Vertex v1(rand() % 65536, rand() % 65536);
        Vertex v2(rand() % 65536, rand() % 65536);

        edges.push_back( ActiveEdge(v1 < v2 ? v1:v2 , v1<v2? v2:v1) );
        //std::cout << edges[i] << endl;
    }

    SimpEventQueue events;

    BOOST_FOREACH( ActiveEdge &edge, edges)
    {
        events.add( SEG_START, edge);
        events.add( SEG_END, edge);
    }

    LineArrangement lines;

    
    while ( events.containsEvents() )
    {
        SimpEvent event = events.pop();
        switch (event.type)
        {
        case SEG_START:
        {
            /* algorithm: 
                1. insert the segment at the correct position in the line arrangement
                   (so that the lines 'physically' directly to its left and right are also 
                   its neighbors in the line arrangement data structure
                2. if there was a scheduled intersection between its predecessor and successor, remove that event
                3. if it potentially intersects with its predecessor or successor, add the corresponding events
                        
             */
            BigInt x_pos = event.m_thisEdge.left.x;
            BigInt y_pos = event.m_thisEdge.left.y;
            ActiveEdge &edge = event.m_thisEdge;
            
            std::cout << "adding edge " << edge << std::endl;
            AVLTreeNode<ActiveEdge>* node = lines.addEdge( edge, x_pos);
            
            //TODO: add edge case where the new line segment overlaps (i.e. is colinear) with an older one
            
            if (lines.hasPredecessor(node) && lines.hasSuccessor(node))
            {
                ActiveEdge pred = lines.getPredecessor(node);
                ActiveEdge succ = lines.getSuccessor(node);
                
                assert (pred.isLessThanOrEqual(edge, x_pos)); /// integrity checks
                assert (edge.isLessThanOrEqual(succ, x_pos)); /// for the line
                assert (pred.isLessThanOrEqual(succ, x_pos)); /// arrangement


                if (intersect(pred, succ))
                {
                    BigFraction x,y;
                                
                    getIntersection(pred, succ, x, y);
                    assert ( (x != x_pos || y != y_pos)  && "not implemented");
                    if ( x > x_pos || (x == x_pos && y > y_pos))
                    {
                        events.remove(INTERSECTION, pred, succ);
                    }
                }
            }
            
            if (lines.hasPredecessor(node))
                scheduleIntersectionIfExists(lines.getPredecessor(node), edge, x_pos, y_pos, events);
            
            if (lines.hasSuccessor(node))
                scheduleIntersectionIfExists(edge, lines.getSuccessor(node), x_pos, y_pos, events);
            
            break;
        }
        case SEG_END:
        {
            /* Algorithm:
                * 1. remove segment from line arrangement
                * 2. if its predecessor and successor have a future intersection, add that event to the queue
            */
                ActiveEdge &edge = event.m_thisEdge;
                
                AVLTreeNode<ActiveEdge> *node = lines.findPos(edge, event.x);
                assert(node);
                
                if (lines.hasPredecessor(node) && lines.hasSuccessor(node))
                    scheduleIntersectionIfExists(lines.getPredecessor(node), lines.getSuccessor(node),
                                                 event.x, event.y, events);
                
                lines.remove(edge, event.x);
                std::cout << "removing edge " << edge << std::endl;
            break;
        }
        case INTERSECTION:
        {
            /* algorithm: 1. report intersection
                          (make sure no other intersection takes plaec at the same position
                          2. remove intersection events of the two intersecting edges with their outer neighbors
                          3. swap the two intersectings edge in the LineArrangement
                          4. re-add new intersection events with their new outer neighbors

            
            */
            std::cout << "intersection between " << event.m_thisEdge << " and " << event.m_otherEdge << std::endl;
            
            #warning CONTINUEHERE: 
            //BigFraction x_pos = event.x;
            //BigFraction y_pos = event.y;
            //ActiveEdge &edge = ;
            assert (event.m_thisEdge.toLineSegment().intersects(event.m_otherEdge.toLineSegment()));
            break;
        }
        default:
            assert(false && "Invalid simplification event type");break;
        }
    }
    
    
/*
    Vertex a(1,0);
    Vertex b(0,1);
    
    TEST( (a-b).squaredLength() == 2);
    TEST( (b-a).squaredLength() == 2);
    //overflow tests for OSM vertices; note that for the longitude (y-coordinate), the last-but-one bit is always zero
    Vertex min(-1800000000, -900000000);
    Vertex max( 1800000000,  900000000);
    TEST( (max-min).squaredLength() == 16200000000000000000ull);
    //a= Vertex(0,0);
    //b= Vertex(1000,1000);
    
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
*/  
}
