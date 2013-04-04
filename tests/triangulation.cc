
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


    for ( list<LineSegment>::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    //for ( LineArrangement::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    {
        cairo_set_source_rgb(cr, 1,rand()/(double)RAND_MAX,rand()/(double)RAND_MAX);
    
        cairo_move_to(cr, asDouble(it->start.get_x()), 200-asDouble(it->start.get_y()));
        cairo_line_to(cr, asDouble(it->end.get_x()), 200-asDouble(it->end.get_y()));
        cairo_stroke( cr);
    }


    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);
}

list<VertexChain> polygonsFromEndMonotoneGraph( map<Vertex, vector<Vertex>> &graph, vector<Vertex> endVertices)
{
    list<VertexChain> res;
    for (vector<Vertex>::const_iterator it = endVertices.begin(); it != endVertices.end(); it++)
    {
        list<Vertex> chain;
        chain.push_back(*it);
        assert(graph[*it].size() == 2);
        
        Vertex v1 = graph[*it][0];
        Vertex v2 = graph[*it][1];
        
        Vertex top = v1.get_y() > v2.get_y() ? v1 : v2;
        Vertex bottom=v1.get_y()> v2.get_y() ? v2 : v1;
        
        while (top != bottom)
        {
            bool updateTop = top.get_x() >= bottom.get_x();
            //Vertex &update = () ? top : bottom;
            Vertex v = updateTop ? top : bottom;
            Vertex pred;
            if (updateTop)
            {
                pred = chain.front();
                chain.push_front(top);
            }
            else
            {
                pred = chain.back();
                chain.push_back(bottom);
            }
            
            assert(graph[v].size() > 1); //at least 2 edges from the original polygon, potentially additional ones from diagonals
            Vertex vSucc;
            bool hasSucc = false;
            
            if (graph[v].size() == 2)
            {
                Vertex next1 = graph[v][0];
                Vertex next2 = graph[v][1];
                assert( (next1 == pred) != (next2 == pred)); //exactly one of the connected vertices has to be the predecessor
                vSucc = (next1 == pred) ? next2 : next1;
                hasSucc = true;
            } else
            {
                vector<Vertex> &candidates = graph[v];

                for ( vector<Vertex>::const_iterator it = candidates.begin(); it != candidates.end(); it++)
                {
                    if (*it == pred) continue;
                    if (it->get_x() > v.get_x()) continue; // otherwise polygon would get a second END-vertex, but each polygon can only have one
                    
                    if (!hasSucc)
                    {
                        hasSucc = true;
                        vSucc = *it;
                        continue;
                    }
                    
                    BigInt dist = it->pseudoDistanceToLine(v , vSucc);
                    if (!updateTop) dist = -dist;   //top vertex chain needs leftmost continuation, bottom chain needs rightmost continuation
                    
                    if (dist < 0)
                    {
                        vSucc = *it;
                    } else if (dist == 0)   //edge case: current vertex and two possible successors lie on a single line
                    {
                        //assert( false && "This code has never been tested");
                        assert( (vSucc.get_x() == v.get_x()) && (it->get_x() == v.get_x()));
                        
                        if ( (updateTop && (it->get_y() < vSucc.get_y())) ||
                            (!updateTop && (it->get_y() > vSucc.get_y())))
                            vSucc = *it;
                    }
                }
            }
            assert(hasSucc);

            if (updateTop)
                top = vSucc;
            else
                bottom= vSucc;                
            
        }
        chain.push_back(top);   //add the final vertex, top == bottom
        chain.push_front(top);  //... and duplicate it to mark it as a closes polygon
        
        res.push_back(VertexChain(chain));        
    }
    return res;
}

void cairo_render_polygons(const list<VertexChain> &polygons)
{

    cairo_surface_t *surface = cairo_pdf_surface_create ("debug1.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    cairo_set_line_width (cr, 0.1);
    cairo_rectangle(cr,1, 200-1, 1, -1);
    cairo_stroke(cr);
    cairo_set_line_width (cr, 0.2);


    
    for (list<VertexChain>::const_iterator it = polygons.begin(); it != polygons.end(); it++)
    {
        vector<Vertex> chain = it->data();
        
        cairo_move_to(cr, asDouble(chain.front().get_x()), 200-asDouble(chain.front().get_y()) );
        for (vector<Vertex>::const_iterator it = chain.begin(); it != chain.end(); it++)
            if (*it != chain.front())
                cairo_line_to(cr, asDouble(it->get_x()), 200-asDouble(it->get_y()) );
                
        cairo_close_path(cr);
        cairo_set_source_rgb(cr, 0,0,0);
        cairo_stroke_preserve(cr);
        //cairo_set_source_rgba(cr, 1,rand()/(double)RAND_MAX,rand()/(double)RAND_MAX, 0.5);
        cairo_set_source_rgba(cr, 1,0,0, 0.5);
        cairo_fill(cr);
    }
    
    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);

}

list<LineSegment> createEndMonotoneDiagonals(const vector<Vertex> verts)
{
    MonotonizeEventQueue events;
    for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);

    map<LineSegment, Vertex> helpers;
    LineArrangement status;
    list<LineSegment> newDiagonals;
    while (events.size() != 0)
    {
        SimpEvent ev = events.pop();
        //cout << "handling event " << ev << endl;
        switch (ev.type)
        {
            case START: 
            {
                assert ( ev.succ.pseudoDistanceToLine(ev.pred, ev.pos) > 0);
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
                   algorithm: 1. remove the right sub-range from 'status' and 'helpers' (since we now only have a single merged range)
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
        
    assert(status.size() == 0);
    assert(helpers.size()== 0);
    return newDiagonals;
}

map<Vertex, vector<Vertex> > toGraph(vector<Vertex> verts, const list<LineSegment> newDiagonals)
{
    map<Vertex, vector<Vertex> >  graph;
    for (uint64_t i = 0; i < verts.size(); i++)
    {
        Vertex v1 = verts[i];
        Vertex v2 = verts[ (i+1) % verts.size() ];
        
        if ( graph.count(v1) == 0) graph.insert( pair<Vertex, vector<Vertex> >(v1, vector<Vertex>() ));
        if ( graph.count(v2) == 0) graph.insert( pair<Vertex, vector<Vertex> >(v2, vector<Vertex>() ));
        
        graph[v1].push_back(v2);
        graph[v2].push_back(v1);
    }
    
    for (list<LineSegment>::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    {
        assert(graph.count(it->start)); //diagonals should only connect to vertices already presend in the 
        assert(graph.count(it->end));   //original polygon
        
        graph[it->start].push_back(it->end);
        graph[it->end].push_back(it->start);
    }
    return graph;
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

    list<LineSegment> newDiagonals = createEndMonotoneDiagonals(verts);
    cout << "========== new edges ==========" << endl;
    BOOST_FOREACH( LineSegment seg, newDiagonals)
        cout << seg << endl;
        
    createCairoDebugOutput( verts, /*status*/newDiagonals);

    cout << "==========================================" << endl;
    map<Vertex, vector<Vertex> > graph = toGraph(verts, newDiagonals);
    vector<Vertex> endVertices;
    
    for (uint64_t i = 0; i < verts.size(); i++)
        if (classifyVertex(verts, i) == END)
            endVertices.push_back(verts[i]);  
    
    list<VertexChain> polygons = polygonsFromEndMonotoneGraph(graph, endVertices);
    cairo_render_polygons(polygons);

    
}






