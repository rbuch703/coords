
#include "triangulation.h"

#include "../vertexchain.h"
#include "../quadtree.h" // for toSimplePolygons()
#include <boost/foreach.hpp>

#include <cairo.h>
#include <cairo-pdf.h>

//void createCairoDebugOutput( vector<Vertex> verts, LineArrangement &newDiagonals )
void createCairoDebugOutput( vector<Vertex> verts, list<LineSegment> &newDiagonals )
{
    MonotonizeEventQueue events;
    for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);


    cairo_surface_t *surface = cairo_pdf_surface_create ("debug.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    //cairo_translate( cr, 500, 500);
    //cairo_scale (cr, 1/1000.0, -1/1000.0);
    cairo_set_line_width (cr, 0.1);

    cairo_rectangle(cr,1, 200-1, 1, -1);
    
    cairo_stroke(cr);
    
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
            case REGULAR:cairo_set_source_rgb(cr, .2,.2,.2); break;
        }
        static const double M_PI = 3.141592;
        cairo_arc(cr, asDouble(event.pos.get_x()), 200-asDouble( event.pos.get_y() ), 1, 0, 2 * M_PI); 
        cairo_fill (cr);
        
        cairo_arc(cr, asDouble(event.pos.get_x()), 200-asDouble( event.pos.get_y() ), 1, 0, 2 * M_PI); 
        cairo_set_source_rgb (cr, 0,0,0);
        cairo_stroke(cr);
    }
        
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to(cr, asDouble(verts[0].get_x()), 200-asDouble(verts[0].get_y()) );
    
    for (uint64_t i = 0; i < verts.size(); i++)
        cairo_line_to(cr, asDouble(verts[i].get_x()), 200-asDouble(verts[i].get_y()) );
    cairo_line_to(    cr, asDouble(verts[0].get_x()), 200-asDouble(verts[0].get_y()) );
    cairo_stroke(cr);


    cairo_set_source_rgb(cr, 1,0,0);
    for ( list<LineSegment>::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    //for ( LineArrangement::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    {
        cairo_move_to(cr, asDouble(it->start.get_x()), 200-asDouble(it->start.get_y()));
        cairo_line_to(cr, asDouble(it->end.get_x()), 200-asDouble(it->end.get_y()));
        cairo_stroke( cr);
    }


    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);
}

int main()
{
    VertexChain p; 

    srand(24);
    for (int i = 0; i < 400; i++)
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

    //createCairoDebugOutput( verts, list<LineSegment>() );
      
    cout << "==========================================" << endl;
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
        cout << "Vertex " << verts[i] << endl;
        //vertex_pos.insert( pair<Vertex, uint64_t>( verts[i], i ));
    }

    
    cout << "=============================" << endl;
    map<LineSegment, Vertex> helpers;
    LineArrangement status;
    list<LineSegment> newDiagonals;
    //static int i = 0;
    while (events.size() != 0)
    {
        //i++;
        //if (i == 81)
        //    createCairoDebugOutput(verts, status);
        SimpEvent ev = events.pop();
        cout << "handling event " << ev << endl;
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
                
                if (ev.pos.get_x() == ev.pred.get_x())  //edge case: status would be a horizontal line, which LineArrangement can't handle
                {
                    //workaround for degenerate case: insert a dummy edge into 'status'
                    //since the next event necessarily has to be the REGULAR event at ev.pred,
                    //the dummy will not be accessed and will be replaced by the REGULAR event
                    LineSegment dummy_edge = LineSegment( Vertex(ev.pos.get_x()-1, ev.pos.get_y()), ev.pos);
                    status.insert( dummy_edge, ev.pos.get_x() );
                    helpers.insert( pair<LineSegment, Vertex>( dummy_edge, ev.pos) );
                    
                } else
                {
                    assert ( ev.pred.pseudoDistanceToLine(ev.succ, ev.pos) > 0);
                    status.insert( LineSegment(ev.pos, ev.pred), ev.pos.get_x() );
                    helpers.insert( pair<LineSegment, Vertex>( LineSegment(ev.pos, ev.pred), ev.pos));
                }
                break;
            }
            case MERGE:
            {
                /* A MERGE event merges two ranges to yield one. It occurs at the vertex connecting the two ranges.
                   TODO: 1. remove the right sub-range from 'status' and 'helpers' (since we now only have a single merged range)
                            This will be a 'status' edge that (in non-degenerate cases) ends at ev.pos
                         2. set ev.pos as the new helper for the 'status' edge of the left sub-range (now the complete range)
                            as ev.pos is not the knwon vertex with the highest x-value inside the range */
                assert( ev.succ.pseudoDistanceToLine( ev.pos, ev.pred) > 0);
                
                Vertex incidentAt = ev.succ.get_x() != ev.pos.get_x() ?
                    ev.pos : ev.succ; /*edge case: if ev.pos and ev.succ have a common x-value, then ev.pos will be 
                                                   the MERGE event, but the status edge will be incident to ev.succ */
                
                LineSegment leftEdge = status.getEdgeLeftOf(incidentAt);
                assert( leftEdge.end == incidentAt );
                
                //assert ( status.findPos( leftEdge, ev.pos.get_x() ) );
                status.remove( leftEdge, ev.pos.get_x() );
                assert( helpers.count(leftEdge) );
                helpers.erase( leftEdge);

                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count(edge));
                helpers[edge] = ev.pos;
                break;
            }
            case REGULAR: //TODO: replace "REGULAR" event by distinct REGULAR_POLY_LEFT und REGULAR_PLY_RIGHT events.
                
                // ensure that colinearities have been removed (e.g. by a prior call to canonicalize())
                assert(  (ev.pred.get_x() != ev.pos.get_x() || ev.succ.get_x() != ev.pos.get_x()) && "Colinear vertices"); 
                if        (ev.pred.get_x() >= ev.pos.get_x() && ev.succ.get_x() <= ev.pos.get_x())
                {   //pred below succ (and polygon is clockwise) --> polygon must be *right* of REGULAR vertex
                    if ( ev.pred.get_x() == ev.pos.get_x() && ev.succ.get_x() < ev.pos.get_x() )
                    {   //edge case: predecessor has identical x value, but successor does not
                        //resolution: do nothing, status and helper are still valid
                    } else
                    {   
                        Vertex prev_status_end = 
                        /* edge case: predecessor has higher x value, but successor does not --> always happens
                         *            at the vertex right after the previous case, i.e. the status ends at the 'succ' vertex*/
                            ( ev.pred.get_x() > ev.pos.get_x() && ev.succ.get_x() == ev.pos.get_x() ) ?
                            ev.succ : ev.pos;
                            
                        EdgeContainer e = status.findAdjacentEdge( prev_status_end );
                        assert (e && (e->m_Data.end == prev_status_end));
                        assert (helpers.count(e->m_Data));
                        helpers.erase( e->m_Data);
                        status.remove( e->m_Data, prev_status_end.get_x());
                        
                        LineSegment newEdge( ev.pos, ev.pred);
                        status.insert( newEdge, ev.pos.get_x() );
                        helpers.insert( pair<LineSegment, Vertex>( newEdge, ev.pos));
                    }
                } else if (ev.pred.get_x() <= ev.pos.get_x() && ev.succ.get_x() >= ev.pos.get_x() )
                {   //pred above succ (and polygon is clockwise) --> polygon must be *left* of REGULAR vertex
                    //assert (ev.pred.get_x() != ev.pos.get_x() && ev.succ.get_x() != ev.pos.get_x() && "Not Implemented");
                    Vertex newHelper = ( ev.pos.get_x() == ev.succ.get_x()) ? ev.succ : ev.pos;
                    
                    LineSegment edge = status.getEdgeLeftOf( newHelper );
                    
                    assert( helpers.count(edge));
                    helpers[edge] = newHelper;
                
                } else assert(false);
                break;
        }
    }
    
    cout << "========== remaining status edges ========" << endl; 
    for (LineArrangement::const_iterator it = status.begin(); it != status.end(); it++)
        cout << *it << endl;
    
    cout << "========== remaining helpers =============" << endl;
    for (map<LineSegment,Vertex>::const_iterator it = helpers.begin(); it != helpers.end(); it++)
        cout << it->first << ", " << it->second << endl;
        
    //assert(status.size() == 0);
    //assert(helpers.size()== 0);
    cout << "========== new edges ==========" << endl;
    BOOST_FOREACH( LineSegment seg, newDiagonals)
        cout << seg << endl;
        
    createCairoDebugOutput( verts, /*status*/newDiagonals);
        
}






