
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

void createCairoDebugOutput( vector<Vertex> verts )
{
    MonotonizeEventQueue events;
    for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);


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
    for (int i = 0; i < 20; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

    list<VertexChain> simples = toSimplePolygons( p );
    VertexChain poly = simples.front();
    BOOST_FOREACH( VertexChain c, simples)
        if ( c.size() > poly.size())
            poly = c;
            
    std::cout << "generated simple polygon with " << poly.size() << " vertices" << std::endl;
    poly.canonicalize();
    
    //#warning ensure that polygon is oriented clockwise. Otherwise the classification will be incorrect
    assert(poly.isClockwise());
        
    vector<Vertex> verts = poly.data();
    assert( verts.front() == verts.back() );
    verts.pop_back();
    
    assert(verts.size() > 3 && "TODO: handle special case were polygon is already a triangle (or degenerated)");
      
    createCairoDebugOutput( verts);
    // prepare the graph representation of the polygon, into which the additional edges can be inserted
    //map<Vertex,vector<Vertex>> graph;
    /*
    for (uint64_t i = 0; i < verts.size(); i++)
    {
        Vertex v1 = verts[i];
        Vertex v2 = verts[ (i+1) % verts.size() ];
        
        if ( graph.count(v1) == 0) graph.insert( pair<Vertex, vector<Vertex> >(v1, vector<Vertex>() ));
        if ( graph.count(v2) == 0) graph.insert( pair<Vertex, vector<Vertex> >(v2, vector<Vertex>() ));
        
        graph[v1].push_back(v2);
        graph[v2].push_back(v1);
    }*/

    MonotonizeEventQueue events;
    //map<Vertex, uint64_t> vertex_pos;
    for (uint64_t i = 0; i < verts.size(); i++)
    {
        events.add( verts, i);
        //vertex_pos.insert( pair<Vertex, uint64_t>( verts[i], i ));
    }
    
    map<LineSegment, Vertex> helpers;
    LineArrangement status;
    list<LineSegment> newDiagonals;
    
    while (events.size() != 0)
    {
        SimpEvent ev = events.pop();
        switch (ev.type)
        {
            case START: 
            {
                assert ( ev.succ.pseudoDistanceToLine(ev.pred, ev.pos) > 0);
                //Vertex left = v1.pseudoDistanceToLine(v2, ev.pos) < 0 ? v1 : v2;
                status.insert( LineSegment(ev.pos, ev.pred), ev.pos.get_x() );
                helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                break;
            }
            case END: 
            {
                EdgeContainer e = status.findAdjacentEdge(ev.pos.get_x(), ev.pos.get_y());
                assert( e->m_Data.start == ev.pos || e->m_Data.end == ev.pos);
                helpers.erase( e->m_Data);
                status.remove( e->m_Data, ev.pos.get_x());
                
                break;
            }
            case SPLIT:
            {
                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count( edge ));
                newDiagonals.push_back( LineSegment( helpers[edge], ev.pos ));
                
                helpers[edge] = ev.pos;
                
                assert ( ev.pred.pseudoDistanceToLine(ev.succ, ev.pos) > 0);
                status.insert( LineSegment(ev.pos, ev.pred), ev.pos.get_x() );
                helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                
                break;
            }
            case MERGE:
            {
                assert( ev.succ.pseudoDistanceToLine( ev.pos, ev.pred) > 0);
                assert ( status.findPos( LineSegment(ev.succ, ev.pos), ev.pos.get_x() ) );
                status.remove( LineSegment(ev.succ, ev.pos), ev.pos.get_x() );
                
                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count(edge));
                helpers[edge] = ev.pos;
                break;
            }
            case REGULAR: //TODO: replace "REGULAR" event by distinct REGULAR_POLY_LEFT und REGULAR_PLY_RIGHT events.
                
                // ensure that colinearities have been removed (e.g. by a prior call to canonicalize())
                assert(  ev.pred.get_x() != ev.pos.get_x() || ev.succ.get_x() != ev.pos.get_x() ); 
                
                if        (ev.pred.get_x() <= ev.pos.get_x() && ev.succ.get_x() >= ev.pos.get_x())
                {   //pred below succ (and polygon is clockwise) --> polygon must be *right* of REGULAR vertex
                    assert (ev.pred.get_x() != ev.pos.get_x() && ev.succ.get_x() != ev.pos.get_x() && "Not Implemented");
                    EdgeContainer e = status.findAdjacentEdge( ev.pos );
                    assert (e && (e->m_Data.start == ev.pos || e->m_Data.end == ev.pos));
                    assert (helpers.count(e->m_Data));
                    helpers.erase( e->m_Data);
                    status.remove( e->m_Data, ev.pos.get_x());
                    
                    LineSegment newEdge( ev.pos, ev.pred);
                    status.insert( newEdge, ev.pos.get_x() );
                    helpers.insert( pair<LineSegment, Vertex>( newEdge, ev.pos));
                    
                } else if (ev.pred.get_x() >= ev.pos.get_x() && ev.succ.get_x() <= ev.pos.get_x() )
                {   //pred above succ (and polygon is clockwise) --> polygon must be *left* of REGULAR vertex
                    assert (ev.pred.get_x() != ev.pos.get_x() && ev.succ.get_x() != ev.pos.get_x() && "Not Implemented");
                    LineSegment edge = status.getEdgeLeftOf( ev.pos );
                    
                    assert( helpers.count(edge));
                    helpers[edge] = ev.pos;
                
                } else assert(false);
                break;
        }
    }
    
}






