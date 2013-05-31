
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
#include "triangulation.h"


list<VertexChain> poly_storage;

mmap_t idx_map = init_mmap ( "intermediate/relations.idx", true, false);
mmap_t data_map= init_mmap ( "intermediate/relations.data",true, false);
uint64_t* offset = (uint64_t*) idx_map.ptr;
uint64_t num_rels = idx_map.size / sizeof(uint64_t);

mmap_t way_idx_map = init_mmap ( "intermediate/ways.idx", true, false);
mmap_t way_data_map= init_mmap ( "intermediate/ways_int.data",true, false);
uint64_t* way_offset = (uint64_t*) way_idx_map.ptr;
//#ifndef NDEBUG
uint64_t num_ways = way_idx_map.size / sizeof(uint64_t);
//#endif



void writePolygonToDisk(std::string path, VertexChain segment)
{
   
    double secs = 0;
    if (segment.size() > 10000)
    {
        secs = getWallTime();
        cout << "\t\tsegment of size " << segment.size();
        cout.flush();
    }

    if (segment.size() < 4) return; // can't be a polygon with less than four vertices (first and last are identical)
    list<VertexChain> simples = toSimplePolygons( segment );
    
    if (segment.size() > 10000)
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
    list<VertexChain> polys = toSimplePolygons( segment );
    //cout << "#" << endl;
    for (list<VertexChain>::iterator it = polys.begin(); it != polys.end(); it = polys.erase(it))
    {
        it->canonicalize();
        if (it->size() < 4) continue;
        /*{
            if (!it->isClockwise())
                it->reverse();
            triangulate( *it);
        }*/
        
        if (it->front() != it->back()) continue;
        poly_storage.push_back(*it);
    }
    
     
    //dumpPolygon(file_base, segment);
}

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
            writePolygonToDisk(file_base+position, seg.data() );
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
    createEmptyFile(file_base+position);
    
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
        if (simp.simplifyArea( (right-(uint64_t)left)/1024 ))
        {
            simp.canonicalize();
            writePolygonToDisk(file_base+position, simp.data() );
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

void reconstructCoastline(FILE * src)//, list<VertexChain> &poly_storage)
{
    PolygonReconstructor recon;
    int idx = 0;
    uint64_t num_self_closed = 0;
    while (! feof(src))
    {
        if (++idx % 10000 == 0) 
            std::cout << (idx/1000) << "k ways read, " << std::endl;
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
    
    clipRecursive( "output/coast/seg", "", poly_storage, -900000000, -1800000000, 900000000, 1800000000);
    
}



/* TODO let this method keep a blacklist of relations already processed through its recursion.
    Currently, relations may be processed several times (and thus cause the creation of multiple polygons),
    and the algorithm may even be caught in an infinite loop.
    */
void addOuterWaysToReconstructor(const OSMRelation &rel, PolygonReconstructor &recon)
{

    for (list<OSMRelationMember>::const_iterator way = rel.members.begin(); way != rel.members.end(); way++)
    {
        if ( (way->role != "outer") && (way->role != "exclave") && (way->role != "")) continue;
        switch(way->type)
        {
            case RELATION:
            {
                if (! offset[way->ref]) continue;
                OSMRelation child_rel( ((uint8_t*)data_map.ptr) + offset[way->ref], way->ref);
                addOuterWaysToReconstructor(child_rel, recon);
                break;            
            }
            case WAY:
            {
                if (way->ref == 0) continue;
                if (way_offset[ way->ref ] == 0) continue;
                const uint8_t * ptr = (uint8_t*)way_data_map.ptr + way_offset[ way->ref];
                OSMIntegratedWay iw(ptr , way->ref);
                VertexChain ch(iw.vertices);
                recon.add(ch);
                break;
            }
            default:
                break;
        }
    }
}


void extractCountries()
{

    
    for (uint64_t rel_id = 0; rel_id < num_rels; rel_id++)
    {
        if (rel_id % 10000 == 0)
            cout << (rel_id/1000) << "k relations read." << endl;   
        if (! offset[rel_id]) continue;
        OSMRelation rel( ((uint8_t*)data_map.ptr) + offset[rel_id], rel_id);
        
        //if (! rel.hasKey("boundary") || rel["boundary"] != "administrative") continue;
        //if (! rel.hasKey("timezone")) continue;
        
        /* current canonical detecting of countries: they have ALL of the following properties:
            - they have an "ISO3166-1" tag
            - they are tagged as admin_level=2
            - they are tagges as boundary="administrative"
            
            all of these have to be true to prevent false positives (e.g. relations that group the boundary segements shared by two countries,
            or territories that are a part of a country, but are still assigned a ISO3166-1 code (e.g. Svalbard)
        */ 
        //if (rel.hasKey("highway")) continue;
        /*
        if (rel.hasKey("route")) continue;
        if (rel["type"] == "multipolygon") continue;    //anything representing a single polygon which either has holes, or to many vertices to be a single WAY
        if (rel["type"] == "restriction") continue;     //restrictions betwwen relations/ways/nodes, e.g. "cannot turn left from here to there"
        if (rel["type"] == "street") continue;          //group of consecutive road segments that share a street name
        if (rel["type"] == "boundary") continue;        //some geographical boundary
        if (rel["type"] == "route_master") continue;    //all the directions and variants of routes (vehicles that always run the same way with the same reference number).
        */
        /*
        cout << rel << endl;
        continue;*/
        
        if ( rel["admin_level"] != "2") continue;
        if ( rel["boundary"] != "administrative") continue;
        if (!rel.hasKey("ISO3166-1")) continue;

        //if (rel["admin_level"] != "4") continue;
        PolygonReconstructor recon;
        
        addOuterWaysToReconstructor(rel, recon);
        
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
    //integrity checke for the mmaps
    assert ( idx_map.size % sizeof(uint64_t) == 0);
    assert ( way_idx_map.size % sizeof(uint64_t) == 0);

    //double allowedDeviation = 100* 40000.0; // about 40km (~1/10000th of the earth circumference)
    //extractNetwork(fopen("intermediate/countries.dump", "rb"), allowedDeviation, "output/country/country");
    //extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    //extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");
    extractCountries();
    //extractBuildings();
    //extractGermany();
/*
    FILE* f = fopen("coastline.dump", "rb");
    if (!f) { std::cout << "Cannot open file \"coastline.dump\"" << std::endl; return 0; }
    reconstructCoastline(f);//, poly_storage);
    */
    //cout << "reconstructed a total of " << poly_storage.size() << " coastline polygons" << endl;
}
