
#include "geometric_types.h"
#include "vertexchain.h"
#include "quadtree.h"
//#include "simplifypolygon.h"
#include <stdlib.h>
#include <assert.h>

#include <iostream>

//#include <boost/foreach.hpp>
#include "fraction.h"

//typedef Fraction<BigInt> BigFrac;
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[1;30m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}


void testOverlapResolution( LineSegment A, LineSegment B, LineSegment res_A, LineSegment res_B)
{
    resolveOverlap(A, B);
    TEST( A == res_A && B == res_B);
}



#define CAIRO_DEBUG_OUTPUT

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

    Vertex A(-298811012,310385291);
    Vertex B(-298695466,310533612);

    Vertex C(-298805191, 310393248);
    Vertex D(-298809204, 310384402);
    

    
    LineSegment S1(A,B);
    LineSegment S2(C,D);
    
    bool b1, b2;
    LineSegment d1,d2;
    handleIntersection(S1,S2,d1,d2,b1, b2);
    
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
    #warning TODO: re-create the simple polygon generation from random line arrangements to test the changes to the simple polygon algorithm
#ifdef CAIRO_DEBUG_OUTPUT
    cairo_surface_t *surface;
    cairo_t *cr;
//    surface = cairo_pdf_surface_create ("debug.pdf", 1500, 2500);
    surface = cairo_pdf_surface_create ("debug.pdf", 1000, 1000);
    cr = cairo_create (surface);
    cairo_translate( cr, 500, 500);
    cairo_scale (cr, 1/1000.0, -1/1000.0);
    //cairo_translate( cr, 500, 200);
    //cairo_scale (cr, 1/1000000.0, 1/1000000.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
#endif 

    FILE* f = fopen("out/huge.poly", "rb");
    uint64_t numVertices;
    int nRead = fread( &numVertices, sizeof(numVertices), 1, f);
    if (nRead != 1) return 0;
    while (numVertices--)
    {
        int32_t v[2];
        int nRead = fread( v, 2 * sizeof(int32_t), 1, f);
        if (nRead != 1) return 0;
        p.append(Vertex(v[0], v[1]));
    }
    
    //p.simplifyArea(100000);
    std::cout << "has " << p.vertices().size() << " vertices" << endl;
    /*
    //TODO: perform multiple polygon simplifications for medium-sized polygons (~500 vertices) under changing random seeds
    srand(24);
    for (int i = 0; i < 200; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

#ifdef CAIRO_DEBUG_OUTPUT    
    //renderPolygon(cr, p.vertices() );
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

/*    Vertex v(-298811012,310385291);
    for (set<Vertex>::const_iterator it = graph[v].begin(); it != graph[v].end(); it++)
    {
        std::cout << "vertex " << v << " is connected to vertex " << *it << std::endl;
    }*/
    

    
    int numEdges = 0;
    
    BigInt X (-298904510);
    BigInt Y ( 310265273);
    cairo_set_source_rgb(cr, 1, 0.7, 0.7);
    cairo_set_line_width(cr, 10);
    int num_nodes = 0;
    for (map<Vertex,set<Vertex>>::const_iterator it = graph.begin(); it != graph.end(); it++)
    {
        if ((abs(X - it->first.x) < 1000000) && abs(Y- it->first.y) < 1000000)
        {
            cairo_set_source_rgb(cr, 1, 0.7, 0.7);
        
           for (set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
           {
                cairo_set_source_rgb(cr, 1, 0.7, 0.7);
                cairo_set_line_width(cr, 10);
                cairo_move_to(cr, asDouble(it->first.x-X), asDouble(it->first.y-Y) );
                cairo_line_to(cr, asDouble(it2->x-X), asDouble(it2->y-Y));
                cairo_stroke(cr);
                num_nodes++;
            }
            
            cairo_arc(cr, asDouble(it->first.x-X), asDouble(it->first.y-Y), 1500, 0, 2 * M_PI);
            cairo_set_source_rgb (cr, 0.2, 0.2, 0.8);
            cairo_fill (cr);
            cairo_stroke(cr);
        }
        
/*           for (set<Vertex>::const_iterator it2 = it->second.begin(); it2 != it->second.end(); it2++)
                cout << "#\t" << it->first << "," << *it2 << endl;
                */
        numEdges += it->second.size();
    }


	

    cairo_arc(cr, asDouble(-298918556 - X), asDouble(310273241 - Y), 1500, 0, 2 * M_PI); //'predecessor'
    cairo_set_source_rgb (cr, 0.8, 0.2, 0.8);
    cairo_fill (cr);
    cairo_stroke(cr);

    cairo_arc(cr, 0, 0, 1500, 0, 2 * M_PI); //'Center'
    cairo_set_source_rgb (cr, 0.2, 0.8, 0.2);
    cairo_fill (cr);
    cairo_stroke(cr);

    cairo_arc(cr, asDouble(-298851625 - X), asDouble(310333159 - Y), 1500, 0, 2 * M_PI); //'successor'
    cairo_set_source_rgb (cr, 0.8, 0.2, 0.2);
    cairo_fill (cr);
    cairo_stroke(cr);

    cairo_arc(cr, asDouble(-298853216 - X), asDouble(310343209 - Y), 1500, 0, 2 * M_PI); //'successor 2'
    cairo_set_source_rgb (cr, 0.8, 0.8, 0.2);
    cairo_fill (cr);
    cairo_stroke(cr);

	    
    cairo_arc(cr, asDouble(-299044300 - X), asDouble(310085832 - Y), 1500, 0, 2 * M_PI); //'predecessor during 2nd approach'
    cairo_set_source_rgb (cr, 0.8, 0.8, 0.8);
    cairo_fill (cr);
    cairo_stroke(cr);
    	
    cairo_arc(cr, asDouble(-298811012 - X), asDouble(310385291 - Y), 1500, 0, 2 * M_PI); 
    cairo_set_source_rgb (cr, 0.7, 0.7, 0.8);
    cairo_fill (cr);
    cairo_stroke(cr);

    cairo_arc(cr, asDouble(-298825118 - X), asDouble(310367184 - Y), 1500, 0, 2 * M_PI); 
    cairo_set_source_rgb (cr, 0.5, 0.5, 0.7);
    cairo_fill (cr);
    cairo_stroke(cr);

    cairo_arc(cr, asDouble(-298809204 - X), asDouble(310384402 - Y), 1500, 0, 2 * M_PI); 
    cairo_set_source_rgb (cr, 0.9, 0.3, 0.3);
    cairo_fill (cr);
    cairo_stroke(cr);
	
    cairo_arc(cr, asDouble(-298805191 - X), asDouble(310393248 - Y), 1500, 0, 2 * M_PI); 
    cairo_set_source_rgb (cr, 0.3, 0.9, 0.3);
    cairo_fill (cr);
    cairo_stroke(cr);
	
   	

    
    cout << num_nodes << " edges rendered to pdf" << endl;

    std::cout << "Connectivity graph consists of " << graph.size() << " vertices and " << numEdges << " edges." << std::endl;

    //return 0;
    
    list<VertexChain> polygons = getPolygons(graph);

    uint64_t num_vertices = 0;
    for (list<VertexChain>::const_iterator it = polygons.begin(); it != polygons.end(); it++)
    {
        num_vertices += it->vertices().size();
        /*
        AABoundingBox box = it->getBoundingBox();
        std::cout << "size: " << it->vertices().size() << ", box: (" << box.width() << ", " << box.height() << ") " << box << endl;
        for (list<Vertex>::const_iterator it2 = it->vertices().begin(); it2 != it->vertices().end(); it2++)
            std::cout << "\t" << *it2 << endl;*/
    }
    std::cout << "generated " << polygons.size() << " polygons with a total of " << num_vertices << " vertices " << endl;
    
    // print final polygons    

    
#ifdef CAIRO_DEBUG_OUTPUT
    //renderPolygons(cr, polygons);
    //renderGraph(cr, graph);
    //cairo_set_source_rgb(cr, 1, 0, 0);
    //cairo_arc (cr, 46, 38, 2, 0, 2*M_PI);
    //cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);
#endif        
    //std::cout << "found " << numIntersections << " intersections on " << intersections.size() 
    //          << " line segments from " << numVertices << " vertices" << std::endl;

}




