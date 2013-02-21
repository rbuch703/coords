
#include "geometric_types.h"
#include "vertexchain.h"
//#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>

#include <boost/foreach.hpp>
#include <stack>
#include "fraction.h"

typedef Fraction<BigInt> BigFrac;
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

Vertex getLeftMostContinuation( const set<Vertex> &vertices, Vertex start, Vertex end)
{
    bool hasMinLeft = false;
    bool hasMinRight = false;
    
    Vertex minLeft, minRight;
    
    for (set<Vertex>::const_iterator it = vertices.begin(); it != vertices.end(); it++)
    {
        BigInt dist = it->pseudoDistanceToLine(start, end);
        if (dist < 0) //lies left of line (start-end)
        {
            if (!hasMinLeft) 
            {
                hasMinLeft = true;
                minLeft = *it;
                continue;
            }
            if (it->pseudoDistanceToLine(end, minLeft) < 0)
                minLeft = *it;
        } else if (dist > 0)    //lies right of line (start-end)
        {
            if (!hasMinRight)
            {
                hasMinRight = true;
                minRight = *it;
                continue;
            }
            if (it->pseudoDistanceToLine(end, minRight) < 0)
                minRight = *it;
        } else
        {
            if (*it == start) continue;
            assert ( LineSegment(start, end).getCoefficient(*it) > BigFraction(1) );
        }
    }
    if (hasMinLeft) return minLeft;
    if (hasMinRight) return minRight;
    
    assert( vertices.count(start) == 1);
    return start;
}

/*
    Returns the polygons that give the outline of the connected graph 'graph'
    Basic idea of the algorithm:
        1. Find the minimum vertex (lexicographically). This one is guaranteed to be part of the outline
        2. Find its one neighboring vertex (from the graph) for which all
           neighboring vertices are on the right side of the edge between the two vertices.
           This vertex is guaranteed to be on the outline as well
        3. From that edge follow the a outline, by turning as far left as possible at each vertex. Whenever
           a vertex is passed twice while following that outline, the vertex chain between the two
           occurrences is extracted as a separate polygon
        4. The algorithm terminates when the starting edge (not just the starting vertex) is visited again.
        
*/
list<VertexChain> getPolygons(map<Vertex,set<Vertex> > &graph)
{
    list<VertexChain> res;

    Vertex initial_start = graph.begin()->first;
    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
        if (it->first < initial_start) initial_start = it->first;
        assert( it->second.count(it->first) == 0); //no vertex must be connected to itself
    }

    //std::cout << "minimum vertex is " << initial_start << std::endl;

    assert( graph.count(initial_start) && (graph[initial_start].size() > 0));
    Vertex initial_end = *graph[initial_start].begin();
    for ( set<Vertex>::const_iterator it = graph[initial_start].begin(); it != graph[initial_start].end(); it++)
    {
        BigInt dist = it->pseudoDistanceToLine(initial_start, initial_end);
        if (dist  < 0) initial_end = *it;
    }
    //std::cout << "leftmost neighbor is " << initial_end << std::endl;

    assert (initial_end > initial_start);
    
    Vertex start = initial_start;
    Vertex end = initial_end;
    set<Vertex> alreadyPassed;
    alreadyPassed.insert(start);
    alreadyPassed.insert(end);
    stack<Vertex> chain;
    chain.push(start);
    chain.push(end);
    
    do
    {
        Vertex cont = getLeftMostContinuation(graph[end], start, end);
        //std::cout << "continue at" <<  cont << std::endl;
        start = end;
        end = cont;
        
        if (alreadyPassed.count(end) > 0)
        {
            //std::cout << "already passed this vertex, backtracking..." << std::endl;
            list<Vertex> polygon;
            polygon.push_front( end);
            while (chain.top() != end)
            {
                //'front' to maintain orientation, since the vertices are removed from the stack in reverse order
                polygon.push_front(chain.top());
                alreadyPassed.erase(chain.top());
                //std::cout << "\t" << chain.top() << std::endl;
                chain.pop();
            }
            assert(polygon.front() != polygon.back());
            polygon.push_front(end);
            assert(polygon.front() == polygon.back());
            
            res.push_back(VertexChain(polygon));
        } else
        {
            chain.push(end);
            alreadyPassed.insert(end);
        }
        
    } while (start != initial_start || end != initial_end);
    
#ifndef NDEBUG
    assert (alreadyPassed.erase(start) == 1);
    assert (alreadyPassed.erase(end) == 1);
    assert (alreadyPassed.size() == 0);
    
    assert (chain.top() == end);
    chain.pop();
    assert (chain.top() == start);
    chain.pop();
    assert(chain.size() == 0);
#endif
    
    return res;
}

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
    #endif
    VertexChain p;
    /*p.append(Vertex(0,1));
    p.append(Vertex(1,0));
    p.append(Vertex(2,0));
    p.append(Vertex(2,4));
    p.append(Vertex(1,4));
    p.append(Vertex(0,3));
    p.append(Vertex(3,2));  //was 1,2
    p.append(Vertex(0,1));
    */
    srand(20);
    for (int i = 0; i < 80; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon
    
    //for (list<Vertex>::const_iterator it = p.vertices().begin(); it != p.vertices().end(); it++)
        //std::cout << it->x << ", " << it->y << std::endl;
    //int numVertices = p.vertices().size() - (p.front() == p.back() ? 1 : 0);
    list<LineSegment> segs;
    const list<Vertex> &verts = p.vertices();
    
    list<Vertex>::const_iterator it2 = verts.begin();
    for (list<Vertex>::const_iterator it = it2++; it2 != verts.end(); it++, it2++)
    {
        segs.push_back( LineSegment(*it, *it2) );
    }
    
    moveIntersectionsToIntegerCoordinates(segs);
    
    TEST( intersectionsOnlyShareEndpoint(segs) );
    map<Vertex,set<Vertex> > graph = getConnectivityGraph(segs);
    
    int numEdges = 0;
    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
        for(set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
            numEdges += it->second.size();
    std::cout << "Connectivity graph consists of " << graph.size() << " vertices and " << numEdges << " edges." << std::endl;
    
    list<VertexChain> polygons = getPolygons(graph);

    std::cout << "generated " << polygons.size() << " polygons" << endl;
    
    //VertexChain 2nd;
    //if (polygons.size() == 1)
    //polygons.pop_front();
    //for (list<Vertex>::const_iterator it = polygons.front().vertices().begin(); it != polygons.front().vertices().end(); it++)
    //    std::cout << it->x << ", " << it->y << std::endl;


    /*
    for (list<VertexChain>::const_iterator it = polygons.begin(); it != polygons.end(); it++)
    {
        for (list<Vertex>::const_iterator v = it->vertices().begin(); v != it->vertices().end(); v++)
            cout << *v << endl;
        cout << "=====" << endl;
    }*/
    //std::cout << "found " << numIntersections << " intersections on " << intersections.size() 
    //          << " line segments from " << numVertices << " vertices" << std::endl;

}




