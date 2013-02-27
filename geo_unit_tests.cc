
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
    //cout << "finding continuation for " << start << " - " << end << endl;
    for (set<Vertex>::const_iterator it = vertices.begin(); it != vertices.end(); it++)
    {
        //cout << "\t testing " << *it << endl;
        BigInt dist = it->pseudoDistanceToLine(start, end);
        //cout << "\t\t pseudodistance is " << dist << endl;
        if (dist < 0) //lies left of line (start-end)
        {
            //cout << "\t\t left " << endl;
            if (!hasMinLeft) 
            {
                hasMinLeft = true;
                minLeft = *it;
                //cout << "\t\t is new minleft" << endl;
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
        } else  //dist == 0
        {
            if (*it == start) continue; //vertex chain start-end-start would not be an continuation, but backtracking
            assert ( LineSegment(start, end).getCoefficient(*it) > BigFraction(1) );
            assert(!hasMinRight || minRight.pseudoDistanceToLine(end, *it) != 0); //there should only be continuation straight ahead
            hasMinRight = true;
            minRight = *it;
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

    std::cout << "minimum vertex is " << initial_start << std::endl;

    assert( graph.count(initial_start) && (graph[initial_start].size() > 0));
    Vertex initial_end = *graph[initial_start].begin();
    for ( set<Vertex>::const_iterator it = graph[initial_start].begin(); it != graph[initial_start].end(); it++)
    {
        BigInt dist = it->pseudoDistanceToLine(initial_start, initial_end);
        if (dist  < 0) initial_end = *it;
    }
    std::cout << "leftmost neighbor is " << initial_end << std::endl;

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

#include <cairo.h>
#include <cairo-pdf.h>

#define M_PI 3.14159265358979323846

void renderGraph(cairo_t *cr, map<Vertex, set<Vertex>> &graph)
{
    
    cairo_set_line_width (cr, 0.1);
    cairo_set_source_rgb (cr, 0, 0, 0);
    
    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
        for (set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
        {
            cairo_move_to (cr, it->first.x, it->first.y);
            cairo_line_to (cr, it2->x, it2->y);
            cairo_stroke(cr);
        }
    }

    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
        cairo_arc(cr, it->first.x, it->first.y, 0.2, 0, 2 * M_PI);
        cairo_set_source_rgb (cr, 1, 0, 0);        
        cairo_fill (cr);
        cairo_stroke(cr);

    }    
    
    cairo_rectangle (cr, 0.25, 0.25, 0.5, 0.5);
    cairo_stroke (cr); 

}

void renderPolygons(cairo_t *cr, list<VertexChain> &polygons)
{
    cairo_set_source_rgb(cr, 0,0,1);
    cairo_set_line_width(cr, 0.5);
    for (list<VertexChain>::const_iterator it = polygons.begin(); it != polygons.end(); it++)
    {
        cairo_move_to( cr, it->vertices().front().x, it->vertices().front().y);
        for (list<Vertex>::const_iterator v = it->vertices().begin(); v != it->vertices().end(); v++)
        {
            cairo_line_to( cr, v->x, v->y);
        }
        cairo_stroke(cr);
        //cout << "=====" << endl;
    }

}

void renderPolygon(cairo_t *cr, const list<Vertex> &vertices)
{
    cairo_set_source_rgb(cr, 1, 0.7, 0.7);
    cairo_set_line_width(cr, 0.2);
    cairo_move_to(cr, vertices.front().x, vertices.front().y);

    for (list<Vertex>::const_iterator it = vertices.begin(); it != vertices.end(); it++)
        cairo_line_to(cr, it->x, it->y);

    cairo_stroke( cr );
}
int main(int, char** )
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
    VertexChain p; /*
    p.append(Vertex(0,1));
    p.append(Vertex(1,0));
    p.append(Vertex(2,0));
    p.append(Vertex(2,4));
    p.append(Vertex(1,4));
    p.append(Vertex(0,3));
    p.append(Vertex(3,2));  //was 1,2
    p.append(Vertex(0,1));
    */

    cairo_surface_t *surface;
    cairo_t *cr;
    surface = cairo_pdf_surface_create ("debug.pdf", 2000, 2000);
    cr = cairo_create (surface);
    cairo_scale (cr, 10, 10);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
        
    srand(24);
    for (int i = 0; i < 20; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/
    
    renderPolygon(cr, p.vertices() );

    /*
    for (list<Vertex>::const_iterator it = p.vertices().begin(); it != p.vertices().end(); it++)
        std::cout << *it << std::endl;
    cout << "========" << endl;*/
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
    {
/*           for (set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
                cout << "#\t" << it->first << "," << *it2 << endl;
                */
        numEdges += it->second.size();
    }
    std::cout << "Connectivity graph consists of " << graph.size() << " vertices and " << numEdges << " edges." << std::endl;

    //return 0;
    list<VertexChain> polygons = getPolygons(graph);

    std::cout << "generated " << polygons.size() << " polygons" << endl;
    
    // print final polygons    
    

    
    renderPolygons(cr, polygons);
    renderGraph(cr, graph);

    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);    
    //std::cout << "found " << numIntersections << " intersections on " << intersections.size() 
    //          << " line segments from " << numVertices << " vertices" << std::endl;

}




