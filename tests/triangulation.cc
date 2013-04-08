
#include "triangulation.h"

#include "../vertexchain.h"
#include "../quadtree.h" // for toSimplePolygons()
#include <boost/foreach.hpp>

#include <cairo.h>
#include <cairo-pdf.h>

//void createCairoDebugOutput( vector<Vertex> verts, LineArrangement &newDiagonals )
void createCairoDebugOutput( const VertexChain &chain, list<LineSegment> &newDiagonals, bool shift=false )
{
    assert(chain.data().front() == chain.data().back());
    vector<Vertex> verts(chain.data());
    verts.pop_back();

    MonotonizeEventQueue events(verts);
    /*for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);*/


    cairo_surface_t *surface = cairo_pdf_surface_create ("debug.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    if (shift)
        cairo_translate( cr, 200, 0);
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

list<VertexChain> polygonsFromEndMonotoneGraph( map<Vertex, vector<Vertex>> graph, vector<Vertex> endVertices)
{
    list<VertexChain> res;
    for (vector<Vertex>::const_iterator it = endVertices.begin(); it != endVertices.end(); it++)
    {
        list<Vertex> chain;
        chain.push_back(*it);
        assert(graph[*it].size() == 2);
        
        Vertex v1 = graph[*it][0];
        Vertex v2 = graph[*it][1];
        
        BigInt dist = v2.pseudoDistanceToLine(*it, v1);
        assert(dist != 0 && "colinear Vertices");
        
        Vertex top =  dist > 0 ? v2 : v1;
        Vertex bottom=dist > 0 ? v1 : v2;

        cout << "\tstarting at vertex " << *it << ", top: " << top << ", bottom: " << bottom << endl;
        
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
            cout << "\tfinding " << ( updateTop ? "top" : "bottom") <<" successor vertex to " << v << endl;
            
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
                        assert( (vSucc.get_x() == v.get_x()) && (it->get_x() == v.get_x()));
                        
                        if ( (updateTop && (it->get_y() < vSucc.get_y())) ||
                            (!updateTop && (it->get_y() > vSucc.get_y())))
                            vSucc = *it;
                    }
                }
            }
            assert(hasSucc);
            cout << "\t\tfound "<< vSucc << endl;

            if (updateTop)
                top = vSucc;
            else
                bottom= vSucc;                
            
        }
        chain.push_back(top);   //add the final vertex, top == bottom
        chain.push_front(top);  //... and duplicate it to mark it as a closes polygon
        
        VertexChain c(chain);
        c.canonicalize();
        //std::cout << c.isClockwise() << endl;
        //std::cout << c.data() << endl;
        assert(c.isClockwise());
        res.push_back(c);        
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

list<LineSegment> createEndMonotoneDiagonals( const VertexChain &chain)
{
    assert( chain.data().front() == chain.data().back());
    vector<Vertex> verts(chain.data());
    verts.pop_back();
    
    assert( verts.size() >= 3);
    MonotonizeEventQueue events(verts);

    /*for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);*/

    map<LineSegment, Vertex> helpers;
    LineArrangement status;
    list<LineSegment> newDiagonals;
    while (events.size() != 0)
    {
        SimpEvent ev = events.pop();
        cout << "handling event " << ev << endl;
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
#ifndef NDEBUG
                if (ev.succ.get_x() == ev.pos.get_x())
                    /* in this edge case the status edge would have to be a horizontal one, which the status BBST cannot deal with.
                     * As a work-around, the horizontal edge was therefore replaced by a dummy edge that passes through the vertex
                     * that caused the previous event (the successor in clockwise vertex order) */
                    assert( e->m_Data.start == ev.succ || e->m_Data.end == ev.succ);
                else
                    assert( e->m_Data.start == ev.pos  || e->m_Data.end == ev.pos);
#endif
                helpers.erase( e->m_Data);
                status.remove( e->m_Data, ev.pos.get_x());
                
                break;
            }
            case SPLIT:
            {
                LineSegment edge = status.getEdgeLeftOf( ev.pos);
                assert( helpers.count( edge ));
                assert(helpers[edge].get_x() <= ev.pos.get_x());
                newDiagonals.push_back( LineSegment( helpers[edge], ev.pos ));
                
                helpers[edge] = ev.pos;
                
                if (ev.pos.get_x() == ev.pred.get_x())  //edge case: status would be a horizontal line, which LineArrangement can't handle
                {
                    //workaround for degenerate case: insert a dummy edge into 'status'
                    //since the next event necessarily has to be the REGULAR event at ev.pred,
                    //the dummy will not be accessed and will be replaced by the REGULAR event
                    LineSegment dummy_edge = LineSegment( Vertex(ev.pos.get_x()-1, ev.pos.get_y()), ev.pos);
                    status.insert( dummy_edge, ev.pos.get_x() );
                    helpers.insert( pair<LineSegment, Vertex>( dummy_edge, Vertex(0,0)) );
                    
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

map<Vertex, vector<Vertex> > toGraph(const VertexChain &chain, const list<LineSegment> newDiagonals)
{
    map<Vertex, vector<Vertex> >  graph;
    assert( chain.data().front() == chain.data().back());
    
    for (uint64_t i = 0; i < chain.size()-1; i++)   //don't need to traverse the last vertex, it's identical to the first one
    {
        Vertex v1 = chain.data()[i];
        Vertex v2 = chain.data()[i+1];
        
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

vector<Vertex> getEndVertices(const VertexChain &chain)
{
    vector<Vertex> poly( chain.data() );
    assert( poly.front() == poly.back());
    poly.pop_back();    //required for the modulus arithmetic in the event classification to work correctly
    
    vector<Vertex> res;
    MonotonizeEventQueue events(poly);
    while (events.containsEvents())
    {
        SimpEvent ev = events.pop();
        if (ev.type == END)
            res.push_back(ev.pos);
    }
    
    /*
    for (uint64_t i = 0; i < poly.size(); i++)
        if (classifyVertex(poly, i) == END)
            res.push_back(poly[i]);  */
 
    return res;
}

list<VertexChain> toMonotonePolygons( const VertexChain &chain)
{
    if (chain.size() == 4) //triangle
    {
        list<VertexChain> res;
        res.push_back( chain );
        return res;
    }
    

/*    cout << "========== new edges ==========" << endl;
    BOOST_FOREACH( LineSegment seg, newDiagonals)
        cout << seg << endl;*/
        

    //cout << "==========================================" << endl;
    //list<LineSegment> dummy;
    //createCairoDebugOutput( chain, dummy);

    list<LineSegment> newDiagonals = createEndMonotoneDiagonals(chain);
    createCairoDebugOutput( chain, newDiagonals);

    list<VertexChain> polygons = polygonsFromEndMonotoneGraph(toGraph(chain, newDiagonals), getEndVertices(chain));

    //createCairoDebugOutput( chain, newDiagonals);
    //cairo_render_polygons(polygons);
    
    list<VertexChain> res;
    for ( list<VertexChain>::iterator poly = polygons.begin(); poly != polygons.end(); poly++)
    {
        static int i = 0;
        i++;
        assert(poly->size() > 3);
        assert(poly->data().front() == poly->data().back());
        if (poly->size() == 4) // edge case: is a triangle (with duplicte end vertex), no further subdivision necessary
        {
            res.push_back( *poly);
            continue;
        }

        //invert x values so that the next execution of createEndMonotoneDiagonals() eliminates MERGE instead of SPLIT vertices
        poly->mirrorX();
        poly->reverse(); // mirroring also inverts the orientation, but this needs to be clockwise for createEndMonotoneDiagonals();
        assert(poly->isClockwise());

        //cout << poly->data() << endl;
        //cout << "===========================" << endl;

        //createCairoDebugOutput( *poly,  dummy, true);
        
        list<LineSegment> diags = createEndMonotoneDiagonals( *poly );
        list<VertexChain> tmp = polygonsFromEndMonotoneGraph(toGraph(*poly, diags), getEndVertices(*poly));

        for ( list<VertexChain>::iterator it = tmp.begin(); it != tmp.end(); it++)
        {
            assert(it->size() > 3); //algorithm should not be able to create degenerate polygons

            it->mirrorX();
            poly->reverse();
            res.push_back( *it );
        }
        
    }
    cairo_render_polygons(res);
    return res;
}

int main()
{
    VertexChain p; 

    srand(24);
    for (int i = 0; i < 500; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon*/

    list<VertexChain> simples = toSimplePolygons( p );
    VertexChain poly = simples.front();
    BOOST_FOREACH( VertexChain c, simples)
        if ( c.size() > poly.size())
            poly = c;
        
    std::cout << "generated simple polygon with " << poly.size() << " vertices" << std::endl;
    poly.canonicalize();
    assert(poly.isClockwise());

    //#warning: TODO: add test cases with very small (3-5 vertices) irregular polygons
    list<VertexChain> polys = toMonotonePolygons (poly.data());   
    std::cout << "generated " << polys.size() << " polygons" << endl;

    
}






