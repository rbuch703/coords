
#include <cairo.h>
#include <cairo-pdf.h>

#include "../triangulation.h"
#include "../quadtree.h" // for toSimplePolygons()

#include <boost/foreach.hpp>

//#include "../vertexchain.h"

void cairo_render_polygons(const list<VertexChain> &polygons)
{
    AABoundingBox box(polygons.front().front());
    BOOST_FOREACH( VertexChain chain, polygons)
        for (uint64_t i = 0; i < chain.size(); i++)
            box += chain.data()[i];
    
    cout << box << endl;
    int64_t width = abs(box.right() - box.left());
    int64_t height= abs(box.bottom() - box.top());
    
    double scale = 200.0 / max(width, height);
    
    double shift_x= box.left();
    double shift_y= min(box.top(), box.bottom());//so that y-values are in range [0.. -height] (since they are mirrored later)
    shift_y += 200/scale;
#define PROJECT(v) ((double)(int32_t)(v).get_x() -shift_x)*scale, -((double)(int32_t)(v).get_y() -shift_y)*scale

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
        
        cairo_move_to(cr, PROJECT( chain.front() ));
        for (vector<Vertex>::const_iterator it = chain.begin(); it != chain.end(); it++)
            if (*it != chain.front())
                cairo_line_to(cr, PROJECT(*it));
                
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
#undef PROJECT
}


//void createCairoDebugOutput( vector<Vertex> verts, LineArrangement &newDiagonals )
void createCairoDebugOutput( const VertexChain &chain, list<LineSegment> &newDiagonals)//, double shift_x, double shift_y, double scale)
{
    AABoundingBox box(chain.front());
    for (uint64_t i = 0; i < chain.size(); i++)
        box += chain.data()[i];
    
    cout << box << endl;
    int64_t width = abs(box.right() - box.left());
    int64_t height= abs(box.bottom() - box.top());
    
    double scale = 200.0 / max(width, height);
    
    double shift_x= box.left();
    double shift_y= min(box.top(), box.bottom());//so that y-values are in range [0.. -height] (since they are mirrored later)
    shift_y += 200/scale;
#define PROJECT(v) ((double)(int32_t)v.get_x() -shift_x)*scale, -((double)(int32_t)v.get_y() -shift_y)*scale
    
    //cout << "scale by " << scale << endl;
    //cout << "shift by (" << shift_x << ", " << shift_y << ")" << endl;
    assert(chain.data().front() == chain.data().back());
    vector<Vertex> verts(chain.data());
    verts.pop_back();

    MonotonizeEventQueue events(verts);
    /*for (uint64_t i = 0; i < verts.size(); i++)
        events.add( verts, i);*/


    cairo_surface_t *surface = cairo_pdf_surface_create ("debug.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); 
    //cairo_scale (cr, scale, scale);

    //cairo_translate( cr, -shift_x, -shift_y);
    
    //cairo_translate( cr, -553625894, +128060048+80*20000);
    


    cairo_set_line_width (cr, 0.1);

    cairo_rectangle(cr,1, 200-1, 1, -1);
    
    cairo_stroke(cr);
    
    cairo_set_line_width (cr, 0.2);
    while (events.size() != 0)
    {
        SimpEvent event = events.pop();
        //double dx = ;
        //double dy = ;
        //cout << "( " << (dx) << ", " << (dy) << ")" << endl;
        switch (event.type)
        {
            case START:  cairo_set_source_rgb(cr, 1,0,0); break;
            case END:    cairo_set_source_rgb(cr, 0,0,1); break;
            case SPLIT:  cairo_set_source_rgb(cr, 1,1,1); break;
            case MERGE:  cairo_set_source_rgb(cr, 0,1,0); break;
            case REGULAR:cairo_set_source_rgb(cr, .2,.2,.2); break;
        }
        static const double M_PI = 3.141592;
        //cairo_arc(cr, asDouble(event.pos.get_x()), -asDouble( event.pos.get_y() ), 1/*/scale*/, 0, 2 * M_PI); 
        cairo_arc(cr, PROJECT(event.pos), 0.5/*/scale*/, 0, 2 * M_PI); 
        cairo_fill_preserve (cr);
        
        //cairo_arc(cr, asDouble(event.pos.get_x()), -asDouble( event.pos.get_y() ), 2000, 0, 2 * M_PI); 
        cairo_set_source_rgb (cr, 0,0,0);
        cairo_stroke(cr);
    }
        
    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_move_to(cr, PROJECT(verts[0]) );
    
    for (uint64_t i = 0; i < verts.size(); i++)
        cairo_line_to(cr, PROJECT(verts[i]) );
    
    cairo_close_path(cr);
    //cairo_line_to(    cr, asDouble(verts[0].get_x()), -asDouble(verts[0].get_y()) );
    cairo_stroke(cr);


    for ( list<LineSegment>::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    //for ( LineArrangement::const_iterator it = newDiagonals.begin(); it != newDiagonals.end(); it++)
    {
        cairo_set_source_rgb(cr, 1,rand()/(double)RAND_MAX,rand()/(double)RAND_MAX);
    
        cairo_move_to(cr, PROJECT(it->start));
        //asDouble(it->start.get_x()), -asDouble(it->start.get_y()));
        cairo_line_to(cr, PROJECT(it->end));
        cairo_stroke( cr);
    }


    cairo_destroy(cr);
    cairo_surface_finish (surface);
    cairo_surface_destroy(surface);
#undef PROJECT    
}

void cairo_render_triangles(const vector<int32_t> &triangles)//, double shift_x, double shift_y, double scale)
{
    AABoundingBox box( Vertex(triangles[0], triangles[1]));
    for (uint64_t i = 0; i < triangles.size(); i+=2)
        box += Vertex(triangles[i], triangles[i+1]);
    
    cout << box << endl;
    int64_t width = abs(box.right() - box.left());
    int64_t height= abs(box.bottom() - box.top());
    
    double scale = 200.0 / max(width, height);
    
    double shift_x= box.left();
    double shift_y= min(box.top(), box.bottom());//so that y-values are in range [0.. -height] (since they are mirrored later)
    shift_y += 200/scale;

#define PROJECT(v) ((double)(int32_t)(v).get_x() -shift_x)*scale, -((double)(int32_t)(v).get_y() -shift_y)*scale

    cairo_surface_t *surface = cairo_pdf_surface_create ("debug2.pdf", 200, 200);
    cairo_t *cr = cairo_create (surface);
    //cairo_scale (cr, scale, scale);

    //cairo_translate( cr, shift_x, shift_y);

    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL); 
    
    cairo_set_line_width (cr, 0.1);
    cairo_rectangle(cr,1, 200-1, 1, -1);
    cairo_stroke(cr);
    cairo_set_line_width (cr, 0.2);

    assert(triangles.size() % 6 == 0);
    
    for (uint64_t i = 0; i +5 < triangles.size(); i+=6)
    {
        Vertex v1(triangles[i  ], triangles[i+1] );
        Vertex v2(triangles[i+2], triangles[i+3] );
        Vertex v3(triangles[i+4], triangles[i+5] );
        cairo_move_to(cr, PROJECT(v1) );
        cairo_line_to(cr, PROJECT(v2) );
        cairo_line_to(cr, PROJECT(v3) );
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
    //createCairoDebugOutput( poly, newDiagonals, -553625894, +128060048+80*20000, 1/10000.0);

    //533059629	-47008165
    createCairoDebugOutput( poly, newDiagonals);//, -533059629, -47008165+200*300, 1/300.0);


    //#warning: TODO: add test cases with very small (3-5 vertices) irregular polygons
    list<VertexChain> polys = toMonotonePolygons (poly.data());   
    std::cout << "generated " << polys.size() << " sub-polygons" << endl;
    cairo_render_polygons(polys);

    //poly = VertexChain();
    vector<int32_t> triangles;
    BOOST_FOREACH( VertexChain c, polys)
    {
        static int i = 0;
        i++;
        cout << "processing vertex chain (" <<i << ")" << endl << c;
        triangulateMonotonePolygon(c, triangles);
    }

    cout << "generated " << (triangles.size() / 6) << " triangles" << endl;
        
//        if ( c.size() > poly.size())
//            poly = c;
    
//    cout << "biggest polygon has " << poly.size()-1 << " vertices: " << endl;
    //cout << poly << endl;
    

    //cairo_render_triangles(triangles,-553625894, +128060048+200*10000, 1/10000.0);
    cairo_render_triangles(triangles);//,-533059629, -47003617+200*10000, 1/10000.0);
    	

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






