
#include "triangulation.h"

#include "../vertexchain.h"
#include "../quadtree.h" // for toSimplePolygons()
#include <boost/foreach.hpp>

#include <cairo.h>
#include <cairo-pdf.h>

/**
    Data Structures:
        - priority queue of events (new segment, end of segment, intersection), 
            - sorted by sweep direction (first by x-coordinate, then by y-coordinate)
            - ability to remove elements in O( log(n) ) (mostly intersections that did not materialize)
            
        - list of active edges (those polygon edges that intersect the current sweep line), 
            - sorted by current y-coordinate
            - ability to swap adjacent edges in O(1) (when they intersect)
            - ability to find edges in O(log(n)) (e.g. the active edge matching the current event)
            - ability to remove edges in O(log(n)) (when they no longer intersec the sweep line)
*/

void createCairoDebugOutput(MonotonizeEventQueue &events, vector<Vertex> verts)
{
    cairo_surface_t *surface = cairo_pdf_surface_create ("debug.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    //cairo_translate( cr, 500, 500);
    //cairo_scale (cr, 1/1000.0, -1/1000.0);
    
    cairo_set_line_width (cr, 0.2);
    
    while (events.size() != 0)
    {
        SimpEvent event = events.pop();
        switch (event.type)
        {
            case START:  cairo_set_source_rgb(cr, 1,0,0); break;
            case END:    cairo_set_source_rgb(cr, 0,0,1); break;
            case SPLIT:  cairo_set_source_rgb(cr, 1,1,1); break;
            case MERGE:  cairo_set_source_rgb(cr, 0,1,0); break;
            case REGULAR:cairo_set_source_rgb(cr, 0,0,0);break;
        }
        static const double M_PI = 3.141592;
        cairo_arc(cr, asDouble(event.pos.get_y()), asDouble( event.pos.get_x() ), 1, 0, 2 * M_PI); 
        cairo_fill (cr);
        
        cairo_arc(cr, asDouble(event.pos.get_y()), asDouble( event.pos.get_x() ), 1, 0, 2 * M_PI); 
        cairo_set_source_rgb (cr, 0,0,0);
        cairo_stroke(cr);
    }
        
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to(cr, asDouble(verts[0].get_y()), asDouble(verts[0].get_x()) );
    
    for (uint64_t i = 0; i < verts.size(); i++)
        cairo_line_to(cr, asDouble(verts[i].get_y()), asDouble(verts[i].get_x()) );
    cairo_line_to(cr, asDouble(verts[0].get_y()), asDouble(verts[0].get_x()) );
    cairo_stroke(cr);

    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);

}

int main()
{
    VertexChain p; 

    srand(24);
    for (int i = 0; i < 100; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

    list<VertexChain> simples = toSimplePolygons( p );
    VertexChain poly = simples.front();
    BOOST_FOREACH( VertexChain c, simples)
        if ( c.size() > poly.size())
            poly = c;
            
    std::cout << "generated simple polygon with " << poly.size() << " vertices" << std::endl;
    poly.canonicalize();
    vector<Vertex> verts = poly.data();
    assert( verts.front() == verts.back() );
    verts.pop_back();
    
    assert(verts.size() > 3 && "TODO: handle special case were polygon is already a triangle (or degenerated)");
    MonotonizeEventQueue events;
    for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);
        
    createCairoDebugOutput( events, verts);
    //==========================
    map<LineSegment, Vertex> helpers;
    LineArrangement status;
}






