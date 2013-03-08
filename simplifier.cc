
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
#include "quadtree.h"


list<VertexChain> poly_storage;


void writePolygonToDisk(std::string path, VertexChain segment)
{
   
    double secs = 0;
    if (segment.vertices().size() > 1000)
    {
        secs = getWallTime();
        cout << "\t\tsegment of size " << segment.vertices().size();
        cout.flush();
    }

    if (segment.vertices().size() < 4) return; // can't be a polygon with less than four vertices (first and last are identical)
    list<VertexChain> simples = toSimplePolygons( segment.vertices() );
    
    if (segment.vertices().size() > 1000)
        std::cout << "\ttook " << (getWallTime() - secs) << " seconds" << endl;
    
    //assert (poly.isSimple());
    //std::cout << poly.isClockwiseHeuristic() << endl;
    while (simples.begin() != simples.end())
    {
        VertexChain poly = simples.front();
        simples.pop_front();
        poly.canonicalize();
        if (poly.vertices().size() < 4) continue;
        dumpPolygon( path, poly.vertices() );
    }
}

void handlePolygon(string, VertexChain& segment)
{
    segment.canonicalize();
    static int count = 0;
    
    count++;
    //FIXME: polygon 463652 increases in size about threefold due to "toSimplePolygons()"
    //FIXME: polygon 463653 [sic] has a size mismatch 
    //FIXME: polygon 305673	is split into 39 simple polygons, could be a bug
    
    /*if (count == 463652) dumpPolygon("out/huge.poly", segment.vertices());
    if (count == 463653) dumpPolygon("out/mismatch.poly", segment.vertices());  */
    
    //int size_before = segment.vertices().size();
    //double  = getTime()
    list<VertexChain> polygons = toSimplePolygons(segment.vertices());
    //int size = 0;
    std::cout << count;
    if (polygons.size() > 10)
        std::cout << "\t split into " << polygons.size() << " simple polygons ";// << size;
    std::cout << std::endl;
    /*segment.canonicalize();
     poly_storage.push_back(segment);*/
     
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
    
    /** maps each vertex that is the start/end point of at least one VertexChain 
        to all the VertexChains that start/end with it **/
    map<Vertex, list<VertexChain*> > borders;    
    
    /** holds pointers to all manually-allocated copies of border segments.
        Only needed to keep track of the existing segments in order to delete them later **/
    set<VertexChain*> border_storage;
    
    int idx = 0;
    while (! feof(src))
    {
        if (++idx % 1 == 0) std::cout << idx << " ways read, " << std::endl;
        
        int i = fgetc(src);
        if (i == EOF) break;
        ungetc(i, src);

        OSMIntegratedWay way(src);
        
        VertexChain s = way.toVertexChain();
        if (s.front() == s.back()) { 
            handlePolygon(base_name, s);
            continue; 
        }
        VertexChain *seg = new VertexChain(s);
        
        borders[seg->front()].push_back(seg);
        borders[seg->back()].push_back (seg);
        border_storage.insert(seg);
    }
    // copy of endpoint vertices from 'borders', so that we can iterate over the endpoints and still safely modify the map
    list<Vertex> endpoints; 
    for (map<Vertex, list<VertexChain*> >::const_iterator v = borders.begin(); v != borders.end(); v++)
        endpoints.push_back(v->first);
    
    cout << "Found " << border_storage.size() << " " << base_name <<" segments and " << zone_entries[base_name] << " loops" << endl;
    idx = 0;
    for (list<Vertex>::const_iterator it = endpoints.begin(); it!= endpoints.end(); it++)
    {
        if (++idx % 1000 == 0) cout << idx/1000 << "k segments joined" << endl;
        Vertex v = *it;
        if (borders[v].size() == 2)  //exactly two border segments meet here --> merge them
        {
            VertexChain *seg1 = borders[v].front();
            VertexChain *seg2 = borders[v].back();        
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

    BOOST_FOREACH (VertexChain* seg, border_storage)
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

void clipFirstComponent(list<VertexChain> &in, int32_t clip_pos, list<VertexChain> &out1, list<VertexChain> &out2)
{
    for (uint64_t i = in.size(); i; i--)
    {
        VertexChain seg = in.front();
        in.pop_front();
        seg.clipFirstComponent( clip_pos, out1, out2);
    }
}

//FIXME: for each recursion, call "toSimplePolygon()" only if the polygon was clipped in the previous recursion step. Otherwise it still is a simple polygon
void clipRecursive(string file_base, string position, list<VertexChain>& segments, 
                   int32_t top, int32_t left, int32_t bottom, int32_t right, uint32_t level = 0)
{
    uint64_t VERTEX_LIMIT = 20000;

    uint64_t num_vertices = 0;
    BOOST_FOREACH( const VertexChain seg, segments)
        num_vertices+= seg.vertices().size();    

    for (uint32_t i = level; i; i--) cout << "  ";
    cout << "processing clipping rect '" << position << "' (" << top << ", " << left << ")-(" << bottom << ", " << right << ") with "
         << num_vertices << " vertices" << endl;
    //if (position != "" && position[0] != '3') return;
    if (segments.size() == 0) return;   //recursion termination

    //cerr << segments.size()

    BOOST_FOREACH( const VertexChain seg, segments)
        assert( seg.vertices().front() == seg.vertices().back());

    
    if (num_vertices < VERTEX_LIMIT)    //few enough vertices to not further subdivide the area
    {
        BOOST_FOREACH( const VertexChain seg, segments)
            writePolygonToDisk(file_base+"#"+position, seg.vertices() );
        return;
    }
    
    BOOST_FOREACH( const VertexChain seg, segments)
    {
        //TODO: remove least significant bits of vertex coordinates after simplification:
        /* the simplified vertices are only shown at a given resolution
         * those least-significant bits that would change the rendered output positions 
         * by less that one pixel may as well be set to zero.
         * This should help better compressing the data (if done later for download), and
         * may provide the basis for only storing 16 bits per coordinate ( from which the 
         * actual positions are computed through global shift and scale values per patch )
        */
        VertexChain simp(seg);
        if (simp.simplifyArea( (right-(uint64_t)left)/2048 ))
        {
            simp.canonicalize();
            writePolygonToDisk(file_base+"#"+position, simp.vertices() );
        }
    }
         
    //if (level == 5)
    //        return;
    
    int32_t mid_h = (left/2) + (right/2);
    int32_t mid_v = (top/2)  + (bottom/2);
    
    list<VertexChain> vLeft, vRight;

    // segments.size() is an O(n) operation in the GNU stl, so better cache it;
    while (segments.begin() != segments.end())
    {
        VertexChain seg = segments.front();
        segments.pop_front();
        /** beware of coordinate semantics: OSM data is stored as (lat,lon)-pairs, where lat is the "vertical" component
          * Thus, compared to computer graphics (x,y)-pairs, the axes are switched */
        seg.clipSecondComponent( mid_h, vLeft, vRight);
    }
    assert(segments.size() == 0);

    BOOST_FOREACH( const VertexChain seg, vLeft)    assert( seg.vertices().front() == seg.vertices().back());
    BOOST_FOREACH( const VertexChain seg, vRight)   assert( seg.vertices().front() == seg.vertices().back());

    list<VertexChain> vTop, vBottom;

    clipFirstComponent(vLeft, mid_v, vTop, vBottom);
    assert(vLeft.size() == 0);
    
    clipRecursive( file_base, position+ "0", vTop,    top,   left, mid_v,  mid_h, level+1);
    vTop.clear();
    clipRecursive( file_base, position+ "2", vBottom, mid_v, left, bottom, mid_h, level+1);
    vBottom.clear();
    
    clipFirstComponent(vRight, mid_v, vTop, vBottom);
    assert(vRight.size() == 0);

    clipRecursive( file_base, position+ "1", vTop,    top,   mid_h, mid_v,  right, level+1);
    vTop.clear();
    clipRecursive( file_base, position+ "3", vBottom, mid_v, mid_h, bottom, right,  level+1);
    vBottom.clear();
    
}

void reconstructCoastline(FILE * src, list<VertexChain> &poly_storage)
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
        
        VertexChain s( way.vertices );
        if (s.front() == s.back())
        {
            handlePolygon("output/coast/tmp", s);
            num_self_closed++;
            continue;
        }
        
        VertexChain* seg = recon.add(s);
        if (seg)
            handlePolygon("output/coast/tmp", *seg);
        recon.clearPolygonList();
    }
    cout << "closed " << poly_storage.size() << " polygons, " << num_self_closed << " were closed in themselves" << endl;
    cout << "heuristically closing incomplete polygons" << endl;
    recon.forceClosePolygons();
    const list<VertexChain>& lst = recon.getClosedPolygons();
    cout << "done, found " << lst.size() << endl;

    BOOST_FOREACH ( VertexChain s, lst)         
        handlePolygon("output/coast/tmp", s);
    //    poly_storage.push_back(s);
}


int main()
{
    //double allowedDeviation = 100* 40000.0; // about 40km (~1/10000th of the earth circumference)
    //extractNetwork(fopen("intermediate/countries.dump", "rb"), allowedDeviation, "output/country/country");
    //extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    //extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");
    FILE* f = fopen("coastline.dump", "rb");
    if (!f) { std::cout << "Cannot open file \"coastline.dump\"" << std::endl; return 0; }
    reconstructCoastline(f, poly_storage);
    cout << "reconstructed a total of " << poly_storage.size() << " coastline polygons" << endl;
    clipRecursive( "output/coast/seg", "", poly_storage, -900000000, -1800000000, 900000000, 1800000000);
}
