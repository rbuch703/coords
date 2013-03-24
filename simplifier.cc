
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
    if (segment.size() > 1000)
    {
        secs = getWallTime();
        cout << "\t\tsegment of size " << segment.size();
        cout.flush();
    }

    if (segment.size() < 4) return; // can't be a polygon with less than four vertices (first and last are identical)
    list<VertexChain> simples = segment.toSimplePolygon( );
    
    if (segment.size() > 1000)
        std::cout << "\ttook " << (getWallTime() - secs) << " seconds" << endl;
    
    //assert (poly.isSimple());
    //std::cout << poly.isClockwiseHeuristic() << endl;
    while (simples.begin() != simples.end())
    {
        VertexChain poly = simples.front();
        simples.pop_front();
        poly.canonicalize();
        if (poly.size() < 4) continue;
        dumpPolygon( path, poly.data() );
    }
}

void handlePolygon(string, VertexChain& segment)
{
    list<VertexChain> polys = segment.toSimplePolygon();
    
    for (list<VertexChain>::iterator it = polys.begin(); it != polys.end(); it = polys.erase(it))
    {
        it->canonicalize();
        if (it->front() != it->back()) continue;
        if (it->size() < 4) continue;
        poly_storage.push_back(*it);
    }
    
     
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

/**
  *
  */
void clipRecursive(string file_base, string position, list<VertexChain>& segments, 
                   int32_t top = -900000000, int32_t left = -1800000000, int32_t bottom = 900000000, int32_t right = 1800000000, uint32_t level = 0)
{
    uint64_t VERTEX_LIMIT = 20000;

    uint64_t num_vertices = 0;
    BOOST_FOREACH( const VertexChain seg, segments)
        num_vertices+= seg.size();    

    for (uint32_t i = level; i; i--) cout << "  ";
    cout << "processing clipping rect '" << position << "' (" << top << ", " << left << ")-(" << bottom << ", " << right << ") with "
         << num_vertices << " vertices" << endl;
    //if (position != "" && position[0] != '3') return;
    if (num_vertices == 0) return;   //recursion termination

    //cerr << segments.size()

    BOOST_FOREACH( const VertexChain seg, segments)
        assert( seg.data().front() == seg.data().back());

    
    if (num_vertices < VERTEX_LIMIT)    //few enough vertices to not further subdivide the area --> write everything to disk
    {
        BOOST_FOREACH( const VertexChain seg, segments)
            writePolygonToDisk(file_base+"#"+position, seg.data() );
        return;
    }
    
    //otherwise: will need subdivision and will only write simplified versions of all polygons to disk
    
    /*HACK: ensure that the file that will contain the simplified vertices exists even when no
     *      vertices are actually written to it (which can happen when the polygons are so small
     *      that all of them are simplified to a single point (are thus no polygons and are not 
     *      stored.
     *      This is necessary, because the renderer uses the existence of a file to determine
     *      whether further sub-tiles exist.
     */
    createEmptyFile(file_base+"#"+position);
    
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
            writePolygonToDisk(file_base+"#"+position, simp.data() );
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

    BOOST_FOREACH( const VertexChain seg, vLeft)    assert( seg.data().front() == seg.data().back());
    BOOST_FOREACH( const VertexChain seg, vRight)   assert( seg.data().front() == seg.data().back());

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

void extractGermany()
{
    mmap_t idx_map = init_mmap ( "intermediate/ways.idx", true, false);
    mmap_t data_map= init_mmap ( "intermediate/ways_int.data",true, false);
    uint64_t* offset = (uint64_t*) idx_map.ptr;
    assert ( idx_map.size % sizeof(uint64_t) == 0);
    uint64_t num_ways = idx_map.size / sizeof(uint64_t);
    
    
    PolygonReconstructor recon;
    
    for (uint64_t way_id = 0; way_id < num_ways; way_id++)
    {
        if (! offset[way_id]) continue;
        OSMIntegratedWay way( ((uint8_t*)data_map.ptr) + offset[way_id], way_id);
        
        if (! way.hasKey("boundary") || way["boundary"] != "administrative") continue;
        if (! way.hasKey("admin_level") || way["admin_level"] != "2") continue;
        if (! way.hasKey("left:country") && ! way.hasKey("right:country")) continue;
        
        bool match = false;
        if (way.hasKey("left:country") && way["left:country"] == "Germany") match = true;
        if (way.hasKey("right:country") && way["right:country"] == "Germany") match = true;
        
        if (!match) continue;
        
        recon.add( VertexChain(way.vertices));
        
        //if (! rel.hasKey("left:country")) continue;
        
        //if (! rel.tags.count( OSMKeyValuePair("boundary", "administrative"))) continue;
        cout << way.id << endl;
    }
    recon.forceClosePolygons();

    list<VertexChain> polys = recon.getClosedPolygons();
    
    for (list<VertexChain>::const_iterator it = polys.begin(); it != polys.end(); it++)
    {
        cout << "polygon with " << it->size() << "vertices" << endl;
    }
    

    clipRecursive( "output/coast/country", "", polys);

}


void extractCountries()
{
    mmap_t idx_map = init_mmap ( "intermediate/relations.idx", true, false);
    mmap_t data_map= init_mmap ( "intermediate/relations.data",true, false);
    uint64_t* offset = (uint64_t*) idx_map.ptr;
    assert ( idx_map.size % sizeof(uint64_t) == 0);
    uint64_t num_rels = idx_map.size / sizeof(uint64_t);

    mmap_t way_idx_map = init_mmap ( "ways.idx", true, false);
    mmap_t way_data_map= init_mmap ( "ways_int.data",true, false);
    uint64_t* way_offset = (uint64_t*) way_idx_map.ptr;
    assert ( way_idx_map.size % sizeof(uint64_t) == 0);
#ifndef NDEBUG
    uint64_t num_ways = way_idx_map.size / sizeof(uint64_t);
#endif

    
    for (uint64_t rel_id = 0; rel_id < num_rels; rel_id++)
    {
        if (rel_id % 1000 == 0)
            cout << (rel_id/1000) << "k relations read." << endl;   
        if (! offset[rel_id]) continue;
        OSMRelation rel( ((uint8_t*)data_map.ptr) + offset[rel_id], rel_id);
        
        //if (! rel.hasKey("boundary") || rel["boundary"] != "administrative") continue;
        //if (! rel.hasKey("timezone")) continue;
        if ( rel["admin_level"] != "2") continue;
        
        
        //if (! rel.tags.count( OSMKeyValuePair("boundary", "administrative"))) continue;
        
        //cout << rel["ISO3166-1"] << ";" << rel["int_name"] << ";" << rel["name:en"] << ";" << rel["iso3166-1:alpha2"] << endl;
        if ( rel.hasKey("ISO3166-1"))
            cout <<  rel["ISO3166-1"] << endl;
        else if (rel.hasKey("iso3166-1") )
            cout <<  rel["iso3166-1"] << endl;
        //else 
        //    continue;
        if (!rel.hasKey("ISO3166-1") && !rel.hasKey("iso3166-1")) continue;
            
        PolygonReconstructor recon;
        
        for (list<OSMRelationMember>::const_iterator way = rel.members.begin(); way != rel.members.end(); way++)
        {
            if (way->type != WAY) continue;
            if (way->role != "outer") continue;
            
            if (way->ref == 0) continue;
            assert(way->ref < num_ways);
            
            if (way_offset[ way->ref ] == 0) continue;
            OSMIntegratedWay iw( (uint8_t*)way_data_map.ptr + way_offset[ way->ref], way->ref);
            VertexChain ch(iw.vertices);
            recon.add(ch);
        }
        
        recon.forceClosePolygons();
        list<VertexChain> polys = recon.getClosedPolygons();
        recon.clear();
        for (list<VertexChain>::iterator it = polys.begin(); it != polys.end(); it++)
        {
            handlePolygon("dummy", *it);
        }
        
        
        //else
        //cout << rel << endl;
    }
    clipRecursive( "output/coast/country", "", poly_storage);   
}

void extractBuildings()
{
    FILE *f = fopen("intermediate/buildings.dump", "rb");
    int i = 0;
    int ch;
    while ( (ch = fgetc(f)) != EOF )
    {
        ungetc(ch, f);
        //int d = fputc(ch, f);
        //perror("fputc");
        //assert(d == ch);
        if (++i % 100000 == 0) cout << (i/1000) << "k buildings read" << endl;
        OSMIntegratedWay way(f, -1);
        if (! (way.vertices.front() == way.vertices.back()))
           way.vertices.push_back(way.vertices.front());
        VertexChain tmp(way.vertices);
        handlePolygon("dummy", tmp);
        //tmp.isClockwise();
        //cout << way << endl;        
    } 
    clipRecursive( "output/coast/building", "", poly_storage);   
    
}

int main()
{
    //double allowedDeviation = 100* 40000.0; // about 40km (~1/10000th of the earth circumference)
    //extractNetwork(fopen("intermediate/countries.dump", "rb"), allowedDeviation, "output/country/country");
    //extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    //extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");
    //extractCountries();
    extractBuildings();
    //extractGermany();
    //FILE* f = fopen("coastline.dump", "rb");
    //if (!f) { std::cout << "Cannot open file \"coastline.dump\"" << std::endl; return 0; }
    //reconstructCoastline(f, poly_storage);
    //cout << "reconstructed a total of " << poly_storage.size() << " coastline polygons" << endl;
    //clipRecursive( "output/coast/seg", "", poly_storage, -900000000, -1800000000, 900000000, 1800000000);
}
