
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

    int num_intersections = 0;
    
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
                
                assert (pred.isLessThan(edge, x_pos)); /// integrity checks for the line arrangement
                assert (edge.isLessThan(succ, x_pos)); /// (less than or equal); and at the same time
                assert (pred.isLessThan(succ, x_pos)); /// assert for not implemented edge case (equal) 


                events.unscheduleIntersection(pred, succ, x_pos, y_pos);
            }
            
            if (lines.hasPredecessor(node))
                events.scheduleIntersection(lines.getPredecessor(node), edge, x_pos, y_pos);
            
            if (lines.hasSuccessor(node))
                events.scheduleIntersection(edge, lines.getSuccessor(node), x_pos, y_pos);
            
            break;
        }
        case SEG_END:
        {
            /* Algorithm:
                * 1. remove segment from line arrangement
                * 2. if its predecessor and successor have a future intersection, add that event to the queue
            */
                ActiveEdge &edge = event.m_thisEdge;
                
                //FIXME: edge case that this segement intersects another segement at this end point
                list<EdgeContainer> nodes = lines.findAllIntersectingEdges(edge, event.x);
                assert(nodes.size() == 1 && "Not implemented");
                
                AVLTreeNode<ActiveEdge> *node = lines.findPos(edge, event.x);
                assert(node);
                
                if (lines.hasPredecessor(node) && lines.hasSuccessor(node))
                    events.scheduleIntersection(lines.getPredecessor(node), lines.getSuccessor(node), event.x, event.y);
                
                lines.remove(edge, event.x);
                std::cout << "removing edge " << edge << std::endl;
            break;
        }
        case INTERSECTION:
        {
            /* algorithm: 1. report intersection
                          FIXME: (make sure no other intersection takes place at the same position
                          2. remove intersection events of the two intersecting edges with their outer neighbors
                          3. swap the two intersectings edge in the LineArrangement
                          4. re-add new intersection events with their new outer neighbors
            */
            assert (event.m_thisEdge.intersects(event.m_otherEdge));
            num_intersections++;
            BigFraction int_x, int_y;
            event.m_thisEdge.getIntersectionWith( event.m_otherEdge, int_x, int_y);

            std::cout << "intersection between " << event.m_thisEdge << " and " << event.m_otherEdge << " at (" << int_x << ", " << int_y << " )" << std::endl;
            
            list<EdgeContainer> nodes = lines.findAllIntersectingEdges(event.m_thisEdge, event.x);
            if (nodes.size() != 2)
            {
                std::cout << "### nodes dump" << std::endl;
                BOOST_FOREACH(AVLTreeNode<ActiveEdge>* node, nodes)
                    std::cout << node->m_Data << std::endl;
            }
            assert( nodes.size() == 2 && "Multiple intersections on a single position are not implemented");
            
            EdgeContainer left = nodes.front();
            EdgeContainer right = nodes.back();
            
            if (lines.hasPredecessor(left))
                events.unscheduleIntersection( lines.getPredecessor(left), left->m_Data, event.x, event.y);
            
            if (lines.hasSuccessor(right))
                events.unscheduleIntersection( right->m_Data, lines.getSuccessor(right), event.x, event.y);

            assert ( left->m_Data.isEqual(right->m_Data, event.x));
            
            ActiveEdge tmp = left->m_Data;
            left->m_Data = right->m_Data;
            right->m_Data = tmp;

            if (lines.hasPredecessor(left))
                events.scheduleIntersection( lines.getPredecessor(left), left->m_Data, event.x, event.y);

            if (lines.hasSuccessor(right))
                events.scheduleIntersection( right->m_Data, lines.getSuccessor(right), event.x, event.y);
            
            break;
        }
        default:
            assert(false && "Invalid event type");break;
        }

        assert( lines.isConsistent( event.x ) );
        
    }
    
    assert( lines.size() == 0);
    assert( events.size() == 0);
    std::cout << "Found " << num_intersections << " intersections " << std::endl;
    
    int n_ints = 0;
    for (list<ActiveEdge>::const_iterator it = edges.begin(); it != edges.end(); it++)
    {
        list<ActiveEdge>::const_iterator it2 = it;
        
        for (it2++; it2 != edges.end(); it2++)
            if (it->intersects(*it2)) n_ints++;
    }

    std::cout << "Found " << n_ints << " intersections " << std::endl;
    
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
