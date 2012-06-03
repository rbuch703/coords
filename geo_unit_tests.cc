
#include "geometric_types.h"

#include <stdlib.h>
#include <assert.h>

#include <fstream>
#include <iostream>
#define TEST(X) { cout << ((X)?"\033[22;32mpassed":"\033[22;31mfailed") << "\033[22;37m \"" << #X <<"\" ("<< "line " << __LINE__ << ")" << endl;}

int main()
{
    Vertex a(1,0);
    Vertex b(0,1);
    
    TEST( (a-b).squaredLength() == 2);
    TEST( (b-a).squaredLength() == 2);
    //overflow tests for OSM vertices; note that for the longitude (y-coordinate), the last-but-one bit is always zero
    Vertex min(-1800000000, -900000000);
    Vertex max( 1800000000,  900000000);
    TEST( (max-min).squaredLength() == 16200000000000000000ull);
    /*
    a= Vertex(0,0);
    b= Vertex(1000,1000);*/
    
    LineSegment seg(min, max, -1, -1);
    TEST( seg.contains(Vertex(0,0) ));
    TEST(!seg.contains(Vertex(1,0)) );
    TEST( seg.contains(Vertex(2, 1) ));
    TEST( seg.contains(Vertex(1000, 500) ));
    TEST(!seg.contains(Vertex(999, 500) ));
    TEST( seg.contains( max - Vertex(2,1)) );
    TEST( seg.contains(min) );
    TEST(!seg.contains(max) );
    
    seg = LineSegment( Vertex(900000000, -900000000), Vertex(-900000000, 900000000), -1, -1);
    TEST( seg.contains(seg.start));
    TEST(!seg.contains(seg.end));
    TEST( seg.contains(Vertex( 0, 0)));
    TEST( seg.contains(Vertex( 1,-1)));
    TEST( seg.contains(Vertex(-1, 1)));
    TEST(!seg.contains(Vertex(-1,-1)));
    TEST(!seg.contains(Vertex( 1, 1)));
    TEST( seg.parallelTo( LineSegment( a-b, b-a, -1, -1)));
    TEST( seg.parallelTo( LineSegment( b-a, a-b, -1, -1)));
    TEST( seg.parallelTo( LineSegment( Vertex(0, 1), Vertex(1, 0), -1, -1)));
    TEST(!seg.parallelTo( LineSegment( Vertex(0, 0), Vertex(1, 1), -1, -1)));
    TEST(!seg.parallelTo( LineSegment( Vertex(-1, -1), Vertex(1, 1), -1, -1)));
    
    seg = LineSegment( Vertex(0,0), Vertex(2,0), -1, -1);
    TEST( Vertex(0, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(-1, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(2, 0).pseudoDistanceToLine( seg.start, seg.end) == 0);
    TEST( Vertex(1, 1).pseudoDistanceToLine( seg.start, seg.end) == -2);
    TEST( Vertex(1,-1).pseudoDistanceToLine( seg.start, seg.end) ==  2);
    
    Vertex A(0,0);
    Vertex B(0,2);
    Vertex C(2,0);
    Vertex D(2,2);
    TEST( LineSegment(A, D, -1, -1).intersects(LineSegment(B, C, -1, -1)));
    TEST( LineSegment(D, A, -1, -1).intersects(LineSegment(C, B, -1, -1)));
    TEST(!LineSegment(A, B, -1, -1).intersects(LineSegment(C, D, -1, -1)));
    TEST(!LineSegment(A, C, -1, -1).intersects(LineSegment(D, B, -1, -1)));

    TEST( LineSegment(A, B, -1, -1).intersects(LineSegment(A, D, -1, -1)));
    TEST(!LineSegment(B, A, -1, -1).intersects(LineSegment(A, D, -1, -1)));
    TEST(!LineSegment(C, D, -1, -1).intersects(LineSegment(B, D, -1, -1))); //only share an endpoint, which is not part of the line
    /*{
        Vertex  f[] = {A, B, C, D};
        list<Vertex> poly(&f[0], &f[4]);
        list<LineSegment> intersections;
        findIntersections( poly, intersections);
        TEST( intersections.size() == 0);
    }
    {
        Vertex E(1, -1);
        Vertex F(1, 3);
        Vertex  f[] = {B, C, D, A, E, F};
        
        list<Vertex> poly(&f[0], &f[6]);
        list<LineSegment> intersections;
        findIntersections( poly, intersections);
        TEST( intersections.size() == 6);
        for (list<LineSegment>::const_iterator it = intersections.begin(); it != intersections.end(); it++)
        {
            cout << it->start << "->" << it->end << endl;
        }
    } */
#if 0    
    list<Vertex> poly;
    for (int a = 0; a < 999; a++)
        poly.push_back( Vertex((int64_t)(rand() / (double)RAND_MAX * 100), (int64_t)(rand() / (double)RAND_MAX * 100) ));
    poly.push_back(poly.front());

    for (list<Vertex>::const_iterator it = poly.begin(); it != poly.end(); it++)
        cout << *it << endl;
    
    cout << endl;
/*    for (list<Vertex>::const_iterator v = poly.begin(); v != poly.end(); v++)
        cout << v->x << ", " << v->y << endl;
    
    cout << "========" << endl;*/
    //list<LineSegment> ints;
    //findIntersections(poly, ints);
//    for (list<LineSegment>::const_iterator ls = ints.begin(); ls != ints.end(); ls++)
//        cout << ls->start << ", start" << endl << ls->end << ", end" << endl;

    list<PolygonSegment> poly_out;
    list<LineSegment> intersections;
    findIntersections(poly, intersections);
    createSimplePolygons(poly, intersections, poly_out);
    
    for (list<PolygonSegment>::const_iterator poly = poly_out.begin(); poly != poly_out.end(); poly++)
    {
        cout << *poly << endl;
    }
#endif

    /*Vertex poly[] = {Vertex(0,300), 
                     Vertex(0,200), 
                     Vertex(200, 0), 
                     Vertex(400, 200), 
                     Vertex(600, 0), 
                     Vertex(800, 200), 
                     Vertex(800,300), 
                     Vertex(0, 300)};
    list<Vertex> l( &poly[0], &poly[8]);*/
    PolygonSegment ps;
    //ps.append(l.begin(), l.end());
    
    ifstream f("output/clipping/clip_20.csv");
    assert(f.good());
    while (!f.eof())
    {
        int32_t lat, lon;
        char c;
        f >> lat >> c >> lon;
        //cout << lat << ", " << lon << endl;
        ps.append(Vertex(lat, lon));
    }
    cout << ps.vertices().size() << " vertices" << endl;
    
    list<PolygonSegment> above, below;
    ps.clipHorizontally(0, above, below);
    cout << "Above: " << endl;
    for (list<PolygonSegment>::const_iterator ps = above.begin(); ps != above.end(); ps++)
        cout << *ps << endl;

    cout << "Below: " << endl;
    for (list<PolygonSegment>::const_iterator ps = below.begin(); ps != below.end(); ps++)
        cout << *ps << endl;
    
    //cout << ints.size()/2 << endl;

}

