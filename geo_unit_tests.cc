
#include "geometric_types.h"
#include "vertexchain.h"
#include "quadtree.h"
//#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>

#include <boost/foreach.hpp>
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



//#define CAIRO_DEBUG_OUTPUT

#ifdef CAIRO_DEBUG_OUTPUT
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
            cairo_move_to (cr, asDouble(it->first.x), asDouble(it->first.y) );
            cairo_line_to (cr, asDouble(it2->x), asDouble(it2->y) );
            cairo_stroke(cr);
        }
    }

    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
        cairo_arc(cr, asDouble(it->first.x), asDouble(it->first.y), 0.2, 0, 2 * M_PI);
        cairo_set_source_rgb (cr, 1, 0, 0);        
        cairo_fill (cr);
        cairo_stroke(cr);

    }    
    
    cairo_rectangle (cr, 0.25, 0.25, 0.5, 0.5);
    cairo_stroke (cr);
}

void renderPolygons(cairo_t *cr, list<VertexChain> &polygons)
{
    cairo_set_line_width(cr, 10000000);
    cairo_set_source_rgb(cr, rand()/(double)RAND_MAX,rand()/(double)RAND_MAX,1);
    for (list<VertexChain>::iterator it = polygons.begin(); it != polygons.end(); it++)
    {
        //it->simplifyArea(100000);
        cairo_move_to( cr, asDouble(it->vertices().front().x), asDouble(it->vertices().front().y));
        for (list<Vertex>::const_iterator v = it->vertices().begin(); v != it->vertices().end(); v++)
        {
            cairo_line_to( cr, asDouble(v->x), asDouble(v->y));
        }
        cairo_stroke(cr);
        //cout << "=====" << endl;
    }

}

void renderPolygon(cairo_t *cr, const list<Vertex> &vertices)
{
    cairo_set_source_rgb(cr, 1, 0.7, 0.7);
    cairo_set_line_width(cr, 1000000);
    cairo_move_to(cr, asDouble(vertices.front().x), asDouble(vertices.front().y));

    for (list<Vertex>::const_iterator it = vertices.begin(); it != vertices.end(); it++)
        cairo_line_to(cr, asDouble(it->x), asDouble(it->y) );

    cairo_stroke( cr );
} 
#endif


int main(int, char** )
{
/*
    BigInt a( getRandom());
    BigInt b = 1;
    b = b << 64;
    a/b;
    return 0;*/
    for (int i = 0; i < 100000; i++)
    {
        BigInt a( getRandom());
        BigInt b( getRandom());
        //cout << i << ", " << b << endl;
        uint32_t c = rand() % 0xFFFFFFFF;
        b = b >> 62;
        a/c;
        a/b;
        //a+c;
    }
    return 0;
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
#ifdef CAIRO_DEBUG_OUTPUT
    cairo_surface_t *surface;
    cairo_t *cr;
    surface = cairo_pdf_surface_create ("debug.pdf", 1500, 2500);
    cr = cairo_create (surface);
    cairo_translate( cr, 500, 200);
    cairo_scale (cr, 1/1000000.0, 1/1000000.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
#endif 

    FILE* f = fopen("out/huge.poly", "rb");
    uint64_t numVertices;
    fread( &numVertices, sizeof(numVertices), 1, f);

    while (numVertices--)
    {
        int32_t v[2];
        fread( v, 2 * sizeof(int32_t), 1, f);
        p.append(Vertex(v[0], v[1]));
    }
    
    //p.simplifyArea(100000);
    std::cout << "has " << p.vertices().size() << " vertices left" << endl;
    /*
    //TODO: perform multiple polygon simplifications for medium-sized polygons (~500 vertices) under changing random seeds
    srand(24);
    for (int i = 0; i < 200; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

#ifdef CAIRO_DEBUG_OUTPUT    
    renderPolygon(cr, p.vertices() );
#endif

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
    
    moveIntersectionsToIntegerCoordinates3(segs);
    //return 0;

    //TEST( intersectionsOnlyShareEndpoint(segs) );
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

    for (list<VertexChain>::const_iterator it = polygons.begin(); it != polygons.end(); it++)
    {
        AABoundingBox box = it->getBoundingBox();
        std::cout << "size: " << it->vertices().size() << ", box: (" << box.width() << ", " << box.height() << ") " << box << endl;
        for (list<Vertex>::const_iterator it2 = it->vertices().begin(); it2 != it->vertices().end(); it2++)
            std::cout << "\t" << *it2 << endl;
    }

    std::cout << "generated " << polygons.size() << " polygons" << endl;
    
    // print final polygons    

    
#ifdef CAIRO_DEBUG_OUTPUT
    //renderPolygons(cr, polygons);
    renderGraph(cr, graph);
    cairo_set_source_rgb(cr, 1, 0, 0);
    cairo_arc (cr, 46, 38, 2, 0, 2*M_PI);
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);
#endif        
    //std::cout << "found " << numIntersections << " intersections on " << intersections.size() 
    //          << " line segments from " << numVertices << " vertices" << std::endl;

}




