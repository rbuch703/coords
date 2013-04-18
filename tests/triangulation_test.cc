
#include <cairo.h>
#include <cairo-pdf.h>

#include "../triangulation.h"
#include "../quadtree.h" // for toSimplePolygons()

#include <boost/foreach.hpp>

//#include "../vertexchain.h"

void cairo_render_polygons(const list<VertexChain> &polygons)
{

    cairo_surface_t *surface = cairo_pdf_surface_create ("debug1.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); 
    
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


//void createCairoDebugOutput( vector<Vertex> verts, LineArrangement &newDiagonals )
void createCairoDebugOutput( const VertexChain &chain, list<LineSegment> &newDiagonals, double shift_x, double shift_y, double scale)
{
    assert(chain.data().front() == chain.data().back());
    vector<Vertex> verts(chain.data());
    verts.pop_back();

    MonotonizeEventQueue events(verts);
    /*for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);*/


    cairo_surface_t *surface = cairo_pdf_surface_create ("debug.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); 
    cairo_scale (cr, scale, scale);

    cairo_translate( cr, shift_x, shift_y);
    //cairo_translate( cr, -553625894, +128060048+80*20000);
    


    cairo_set_line_width (cr, 0.1);

    cairo_rectangle(cr,1, 200-1, 1, -1);
    
    cairo_stroke(cr);
    
    cairo_set_line_width (cr, 2);
    
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
        cairo_arc(cr, asDouble(event.pos.get_x()), -asDouble( event.pos.get_y() ), 2000, 0, 2 * M_PI); 
        cairo_fill (cr);
        
        cairo_arc(cr, asDouble(event.pos.get_x()), -asDouble( event.pos.get_y() ), 2000, 0, 2 * M_PI); 
        cairo_set_source_rgb (cr, 0,0,0);
        cairo_stroke(cr);
    }
        
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to(cr, asDouble(verts[0].get_x()), -asDouble(verts[0].get_y()) );
    
    for (uint64_t i = 0; i < verts.size(); i++)
        cairo_line_to(cr, asDouble(verts[i].get_x()), -asDouble(verts[i].get_y()) );
    cairo_line_to(    cr, asDouble(verts[0].get_x()), -asDouble(verts[0].get_y()) );
    cairo_stroke(cr);


    for ( list<LineSegment>::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    //for ( LineArrangement::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    {
        cairo_set_source_rgb(cr, 1,rand()/(double)RAND_MAX,rand()/(double)RAND_MAX);
    
        cairo_move_to(cr, asDouble(it->start.get_x()), -asDouble(it->start.get_y()));
        cairo_line_to(cr, asDouble(it->end.get_x()), -asDouble(it->end.get_y()));
        cairo_stroke( cr);
    }


    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);
}

void cairo_render_triangles(const vector<int32_t> &triangles, double shift_x, double shift_y, double scale)
{

    cairo_surface_t *surface = cairo_pdf_surface_create ("debug2.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    cairo_scale (cr, scale, scale);

    cairo_translate( cr, shift_x, shift_y);

    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); 
    
    cairo_set_line_width (cr, 0.1);
    cairo_rectangle(cr,1, 200-1, 1, -1);
    cairo_stroke(cr);
    cairo_set_line_width (cr, 0.2);

    assert(triangles.size() % 6 == 0);
    
    for (uint64_t i = 0; i +5 < triangles.size(); i+=6)
    {
        cairo_move_to(cr, triangles[i  ], -triangles[i+1] );
        cairo_line_to(cr, triangles[i+2], -triangles[i+3] );
        cairo_line_to(cr, triangles[i+4], -triangles[i+5] );
        cairo_close_path(cr);
        cairo_set_source_rgb(cr, 0,0,0);
        cairo_stroke_preserve(cr);
        //cairo_set_source_rgba(cr, 1,rand()/(double)RAND_MAX,rand()/(double)RAND_MAX, 0.5);
        cairo_set_source_rgba(cr, 0,0.5,1, 0.5);
        cairo_fill(cr);
    }
    
    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);

}


int main()
{
    
/*
    VertexChain p; 
    srand(24);
    for (int i = 0; i < 500; i++)
        p.append(Vertex(rand() % 200, rand() % 200));
    p.append(p.front()); //close polygon

    list<VertexChain> simples = toSimplePolygons( p );
    VertexChain poly = simples.front();
    BOOST_FOREACH( VertexChain c, simples)
        if ( c.size() > poly.size())
            poly = c;
        
    std::cout << "generated simple polygon with " << (poly.size()-1) << " vertices" << std::endl;
    poly.canonicalize();
    assert(poly.isClockwise());*/
    
    VertexChain poly;
    FILE* f = fopen("poly.bin", "rb");
    while (true)
    {
        int32_t x,y;
        fread(&x, sizeof(x), 1, f);
        fread(&y, sizeof(y), 1, f);
        if (feof(f)) break; //only returns 'true' after the first non-existent byte has been read
        poly.append( Vertex(x,y));
        
    }

    list<LineSegment> newDiagonals = createEndMonotoneDiagonals(poly);
    createCairoDebugOutput( poly, newDiagonals, -553625894, +128060048+80*20000, 1/10000.0);


    //#warning: TODO: add test cases with very small (3-5 vertices) irregular polygons
    list<VertexChain> polys = toMonotonePolygons (poly.data());   
    std::cout << "generated " << polys.size() << " sub-polygons" << endl;

    //poly = VertexChain();
    vector<int32_t> triangles;
    
    BOOST_FOREACH( VertexChain c, polys)
    {
        //cout << "processing vertex chain " << endl << c;
        //static int i = 0;
        //i++;
        triangulateMonotonePolygon(c, triangles);
    }

    cout << "generated " << (triangles.size() / 6) << " triangles" << endl;
        
//        if ( c.size() > poly.size())
//            poly = c;
    
//    cout << "biggest polygon has " << poly.size()-1 << " vertices: " << endl;
    //cout << poly << endl;
    

    cairo_render_triangles(triangles,-553625894, +128060048+200*10000, 1/10000.0);
    /*
    VertexChain q;  // tests the case of two adjacent vertices with identical x-value forming a REGULAR-START pair
    q.append( Vertex(0,0));
    q.append( Vertex(0,1));
    q.append( Vertex(2,2));
    q.append( Vertex(3,1));
    q.append( Vertex(3,0));
    q.append( Vertex(0,0));
    toMonotonePolygons (q.data());   
    */
}






