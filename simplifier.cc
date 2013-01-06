
#include <stdio.h>
#include <assert.h>

#include <set>
#include <list>
#include <fstream>
#include <iostream>

#include <boost/foreach.hpp>

#include "osm_types.h"
#include "polygonreconstructor.h"
#include "helpers.h"

map<string, uint32_t> zone_entries;



void dumpPolygon(string file_base, const PolygonSegment& segment)
{
    size_t pos = file_base.rfind('/');
    string directory = file_base.substr(0, pos);
    ensureDirectoryExists(directory);
    
    PolygonSegment poly(segment);
    //assert (poly.isSimple());
    poly.canonicalize();
    //std::cout << poly.isClockwiseHeuristic() << endl;
    if (poly.vertices().size() < 4) return;
   
    FILE* f = fopen(file_base.c_str(), "ab");
    uint64_t nVertices = poly.vertices().size();
    fwrite( &nVertices, sizeof(nVertices), 1, f);
    
    BOOST_FOREACH( const Vertex vertex, poly.vertices())
    {
        BigInt val = vertex.x;
        fwrite(&val, sizeof(val), 1, f);
        val = vertex.y;
        fwrite(&val, sizeof(val), 1, f);
        
    }
    
    /*BOOST_FOREACH( const Vertex vertex, segment.vertices())
        out << vertex.x << ", " << vertex.y << endl;

    out.close();*/
    
    fclose(f);
}



list<PolygonSegment> poly_storage;
 

void handlePolygon(string, PolygonSegment& segment)
{
    poly_storage.push_back(segment);

    //#warning FIXME: filter out those "polygons" that have less than four vertices
    //dumpPolygon(file_base, segment);

}

#if 0
/** 
 *  @brief: 
 *  @value base_name: the base file name of the output; a serial number and the extension ".csv" are appended
 */
 
void extractNetwork(FILE* src/*, double allowedDeviation*/, string base_name)
{
    
    /** maps each vertex that is the start/end point of at least one PolygonSegment 
        to all the PolygonSegments that start/end with it **/
    map<Vertex, list<PolygonSegment*> > borders;    
    
    /** holds pointers to all manually-allocated copies of border segments.
        Only needed to keep track of the existing segments in order to delete them later **/
    set<PolygonSegment*> border_storage;
    
    int idx = 0;
    while (! feof(src))
    {
        if (++idx % 1 == 0) std::cout << idx << " ways read, " << std::endl;
        
        int i = fgetc(src);
        if (i == EOF) break;
        ungetc(i, src);

        OSMIntegratedWay way(src);
        
        PolygonSegment s = way.toPolygonSegment();
        if (s.front() == s.back()) { 
            handlePolygon(base_name, s);
            continue; 
        }
        PolygonSegment *seg = new PolygonSegment(s);
        
        borders[seg->front()].push_back(seg);
        borders[seg->back()].push_back (seg);
        border_storage.insert(seg);
    }
    // copy of endpoint vertices from 'borders', so that we can iterate over the endpoints and still safely modify the map
    list<Vertex> endpoints; 
    for (map<Vertex, list<PolygonSegment*> >::const_iterator v = borders.begin(); v != borders.end(); v++)
        endpoints.push_back(v->first);
    
    cout << "Found " << border_storage.size() << " " << base_name <<" segments and " << zone_entries[base_name] << " loops" << endl;
    idx = 0;
    for (list<Vertex>::const_iterator it = endpoints.begin(); it!= endpoints.end(); it++)
    {
        if (++idx % 1000 == 0) cout << idx/1000 << "k segments joined" << endl;
        Vertex v = *it;
        if (borders[v].size() == 2)  //exactly two border segments meet here --> merge them
        {
            PolygonSegment *seg1 = borders[v].front();
            PolygonSegment *seg2 = borders[v].back();        
            borders[ seg1->front()].remove(seg1);
            borders[ seg1->back() ].remove(seg1);
            borders[ seg2->front()].remove(seg2);
            borders[ seg2->back() ].remove(seg2);
            border_storage.erase(seg2);

            if      (seg1->front() == seg2->front()) { seg1->reverse(); seg1->append(*seg2, true); }
            else if (seg1->back()  == seg2->front()) {                  seg1->append(*seg2, true); }
            else if (seg1->front() == seg2->back() ) { seg2->append(*seg1, true); *seg1 = *seg2;   }
            else if (seg1->back()  == seg2->back() ) { seg2->reverse(); seg1->append(*seg2, true); }
            else assert(false);
            delete seg2;
            if (seg1->front() == seg1->back())
            {
                border_storage.erase(seg1);
                handlePolygon(base_name, *seg1);
                delete seg1;
                continue; 
            }
            borders[seg1->front()].push_back(seg1);
            borders[seg1->back() ].push_back(seg1);
        }
    }
    cout << "Merged them to " << border_storage.size() << " " << base_name << " segments" << endl;
    idx = 0;
    cout << "now processing segments that could not be concatenated to form polygons" << endl;

    BOOST_FOREACH (PolygonSegment* seg, border_storage)
    {
        /*
        if (++idx % 1000 == 0) std::cout << idx/1000 << "k segments simplified, " << std::endl;
        seg->simplifyStroke(allowedDeviation);
        assert(seg->front() != seg->back()); //if both are equal it should not be in this list
        dumpPolygon(base_name, *seg);
        */
        delete seg;
    }
    
    cout << "Altogether " << zone_entries[base_name] << " "<< base_name <<" boundary segments (including loops)" << endl;
    
}
#endif
void clipRecursive(string file_base, string position, list<PolygonSegment>& segments, 
                   int32_t top, int32_t left, int32_t bottom, int32_t right, uint32_t level = 0)
{
    uint64_t VERTEX_LIMIT = 20000;

    uint64_t num_vertices = 0;
    BOOST_FOREACH( const PolygonSegment seg, segments)
        num_vertices+= seg.vertices().size();    

    for (uint32_t i = level; i; i--) cout << "  ";
    cout << "processing clipping rect '" << position << "' (" << top << ", " << left << ")-(" << bottom << ", " << right << ") with "
         << num_vertices << " vertices" << endl;
    //if (position != "" && position[0] != '3') return;
    if (segments.size() == 0) return;   //recursion termination

    BOOST_FOREACH( const PolygonSegment seg, segments)
        assert( seg.vertices().front() == seg.vertices().back());

    
    if (num_vertices < VERTEX_LIMIT)    //few enough vertices to not further subdivide the area
    {
        BOOST_FOREACH( const PolygonSegment seg, segments)
            dumpPolygon(file_base+"#"+position, seg);
        return;
    }
    
    BOOST_FOREACH( const PolygonSegment seg, segments)
    {
        //TODO: remove least significant bits of vertex coordinates after simplification:
        /* the simplified vertices are only shown at a given resolution
         * those least-significant bits that would change the rendered output positions 
         * by less that one pixel may as well be set to zero.
         * This should help better compressing the data (if done later for download), and
         * may provide the basis for only storing 16 bits per coordinate ( from which the 
         * actual positions are computed through global shift and scale values per patch )
        */
        PolygonSegment simp(seg);
        if (simp.simplifyArea( (right-(uint64_t)left)/2048 ))
            dumpPolygon(file_base+"#"+position, simp);
    }
         
    //if (level == 5)
    //        return;
    
    int32_t mid_h = (left/2) + (right/2);
    int32_t mid_v = (top/2)  + (bottom/2);
    
    list<PolygonSegment> vLeft, vRight;

    /** beware of coordinate semantics: OSM data is stored as (lat,lon)-pairs, where lat is the "vertical" component
      * Thus, compared to computer graphics (x,y)-pairs, the coordinates are switched */
    BOOST_FOREACH( const PolygonSegment seg, segments)
        seg.clipSecondComponent( mid_h, vLeft, vRight);

    BOOST_FOREACH( const PolygonSegment seg, vLeft)    assert( seg.vertices().front() == seg.vertices().back());
    BOOST_FOREACH( const PolygonSegment seg, vRight)   assert( seg.vertices().front() == seg.vertices().back());

    list<PolygonSegment> vTop, vBottom;

    //process left half (top-left and bottom-left quarters)    
    BOOST_FOREACH( const PolygonSegment seg, vLeft)
        seg.clipFirstComponent( mid_v, vTop, vBottom);
    
    clipRecursive( file_base, position+ "0", vTop,    top,   left, mid_v,  mid_h, level+1);
    clipRecursive( file_base, position+ "2", vBottom, mid_v, left, bottom, mid_h, level+1);
    vTop.clear();
    vBottom.clear();
    
    //process right half (top-right and bottom-right quarters)
    BOOST_FOREACH( const PolygonSegment seg, vRight)
        seg.clipFirstComponent( mid_v, vTop, vBottom);

    clipRecursive( file_base, position+ "1", vTop,    top,   mid_h, mid_v,  right, level+1);
    clipRecursive( file_base, position+ "3", vBottom, mid_v, mid_h, bottom, right,  level+1);
    
    //list<PolygonSegment> above, below;
    
    //segment.clipHorizontally(40*10000000, above, below);
    /*segment.clipHorizontally(0, above, below);
    if (above.size() == 0 || below.size() == 0) return; // no clipping took place

    for (list<PolygonSegment>::const_iterator it = above.begin(); it != above.end(); it++)
        dumpPolygon(file_base, *it);

    for (list<PolygonSegment>::const_iterator it = below.begin(); it != below.end(); it++)
        dumpPolygon(file_base, *it);
    */    
    //if (segment.simplifyArea(100*1000))// && segment.vertices().size() > 1000)

}

void reconstructCoastline(FILE * src, list<PolygonSegment> &poly_storage)
{
    PolygonReconstructor recon;
    int idx = 0;
    uint64_t num_self_closed = 0;
    while (! feof(src))
    {
        if (++idx % 100000 == 0) std::cout << idx/1000 << "k ways read, " << std::endl;
        int i = fgetc(src);
        if (i == EOF) break;
        ungetc(i, src);
        OSMIntegratedWay way(src);
        if (way["natural"] != "coastline")
            cout << way << endl;
        assert (way.hasKey("natural") && way["natural"] == "coastline");
        
        PolygonSegment s( way.vertices );
        if (s.front() == s.back())
        {
            handlePolygon("output/coast/tmp", s);
            num_self_closed++;
            continue;
        }
        
        PolygonSegment* seg = recon.add(s);
        if (seg)
            handlePolygon("output/coast/tmp", *seg);
        recon.clearPolygonList();
    }
    cout << "closed " << poly_storage.size() << " polygons, " << num_self_closed << " were closed in themselves" << endl;
    cout << "closing polygons" << endl;
    recon.forceClosePolygons();
    const list<PolygonSegment>& lst = recon.getClosedPolygons();
    cout << "done, found " << lst.size() << endl;

    BOOST_FOREACH ( PolygonSegment s, lst)         
        handlePolygon("output/coast/tmp", s);
    //    poly_storage.push_back(s);
}


int main()
{
    //double allowedDeviation = 100* 40000.0; // about 40km (~1/10000th of the earth circumference)
    //extractNetwork(fopen("intermediate/countries.dump", "rb"), allowedDeviation, "output/country/country");
    //extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    //extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");
    reconstructCoastline(fopen("coastline.dump", "rb"), poly_storage);
    cout << "reconstructed a total of " << poly_storage.size() << " coastline polygons" << endl;
    clipRecursive( "output/coast/seg", "", poly_storage, -900000000, -1800000000, 900000000, 1800000000);
}
