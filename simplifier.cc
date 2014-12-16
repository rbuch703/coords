
#include <stdio.h>
#include <assert.h>

#include <set>
#include <list>
#include <fstream>
#include <iostream>

//#include <boost/foreach.hpp>

#include "osm_types.h"
#include <polygonreconstructor.h>
#include <helpers.h>
#include <quadtree.h>
//#include <triangulation.h>


list<VertexChain> poly_storage;

VertexChain createVertexChain( const list<OSMVertex> &vertices)
{
    VertexChain vc;
    for( const OSMVertex &v: vertices)
        vc.append( Vertex( v.x, v.y));
        
    return vc;
}



class OSMEntities
{

public:
    OSMEntities(string indexFilename, string  dataFileName)
    {
        idx_map = init_mmap ( indexFilename.c_str(),true, false);
        data_map = init_mmap ( dataFileName.c_str(),true, false);
        num = idx_map.size / sizeof(uint64_t);

    }

        
    ~OSMEntities()
    {
        free_mmap (&idx_map);
        free_mmap (&data_map);
    }
    
    bool exists(uint64_t id) { 
        if (id >= num) return false;
        return ((uint64_t*)idx_map.ptr)[id];
    }
    /*
    OSMNode get(uint64_t node_id)
    {
        assert ( node_id >= num_nodes );
        uint64_t idx = ((uint64_t*)node_idx_map.ptr)[node_id];
        assert( idx > 0);
        return OSMNode( ((uint8_t*)node_data_map.ptr) + idx, node_id);
    }*/
    
    uint64_t count() const { return num;}
protected:
    uint64_t num;
    mmap_t idx_map;
    mmap_t data_map;

};


class Nodes: public OSMEntities
{

public:
    Nodes(string indexFilename, string  dataFileName): OSMEntities( indexFilename, dataFileName) {}


    OSMNode get(uint64_t id)
    {
        assert ( id <= num );
        uint64_t idx = ((uint64_t*) this->idx_map.ptr)[id];
        assert( idx > 0);
        return OSMNode( ((uint8_t*)data_map.ptr) + idx, id);
    }
};

class Ways: public OSMEntities
{

public:
    Ways(string indexFilename, string  dataFileName): OSMEntities( indexFilename, dataFileName) {}

    OSMIntegratedWay get(uint64_t way_id)
    {
        assert ( way_id <= num );
        uint64_t idx = ((uint64_t*)idx_map.ptr)[way_id];
        assert( idx > 0);
        const uint8_t* dest = ((uint8_t*)data_map.ptr) + idx;
        return OSMIntegratedWay( dest, way_id);
    }

};

class Relations: public OSMEntities
{

public:
    Relations(string indexFilename, string  dataFileName): OSMEntities( indexFilename, dataFileName) {}

    OSMRelation get(uint64_t rel_id)
    {
        if (rel_id == 0)
            return OSMRelation(-1, list<OSMRelationMember>(), vector<OSMKeyValuePair>() );
            
        assert ( rel_id <= num );
        uint64_t idx = ((uint64_t*)idx_map.ptr)[rel_id];
        if (idx == 0)
            return OSMRelation(-1, list<OSMRelationMember>(), vector<OSMKeyValuePair>() );
        
        assert( idx > 0);
        const uint8_t* dest = ((uint8_t*)data_map.ptr) + idx;
        return OSMRelation( dest, rel_id);
    }

};

void simplifyGraph(list<VertexChain> &data, double allowedDeviation); // forward


void writePolygonToDisk(std::string path, VertexChain segment, bool closeSegments)
{
   
    if (!closeSegments) //just dump the line segment, no need for conversion to simple polygon, etc.
    {
        segment.canonicalize();
        if (segment.size() > 1)
            dumpPolygon( path, segment.data() );
        return;
        
    }
   
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



void clipFirstComponent(list<VertexChain> &in, int32_t clip_pos, bool closeSegments, list<VertexChain> &out1, list<VertexChain> &out2)
{
    for (uint64_t i = in.size(); i; i--)
    {
        VertexChain seg = in.front();
        in.pop_front();
        seg.clipFirstComponent( clip_pos, closeSegments, out1, out2);
    }
}

set<string> leaves;

void clipToQuadrants(list<VertexChain> &segments, int32_t mid_x, int32_t mid_y, bool closeSegments, bool clipDestructive,
                     list<VertexChain> &vTopLeft,    list<VertexChain> &vTopRight, 
                     list<VertexChain> &vBottomLeft, list<VertexChain> &vBottomRight)
{
    list<VertexChain> vLeft, vRight;

    if (clipDestructive)    // saves memory, but destroys the content of 'segments'
    {
        while (segments.begin() != segments.end())
        {
            VertexChain seg = segments.front();
            segments.pop_front();
            /** beware of coordinate semantics: OSM data is stored as (lat,lon)-pairs, where lat is the "vertical" component
              * Thus, compared to computer graphics (x,y)-pairs, the axes are switched */
            seg.clipSecondComponent( mid_x, closeSegments, vLeft, vRight);
        }
        assert(segments.size() == 0);
    } else
    {
        for (VertexChain &seg: segments)
            seg.clipSecondComponent( mid_x, closeSegments, vLeft, vRight);
    }
    
    if (closeSegments)
    {
    #ifndef NDEBUG
        for ( const VertexChain &seg: vLeft)    assert( seg.data().front() == seg.data().back());
        for ( const VertexChain &seg: vRight)   assert( seg.data().front() == seg.data().back());
    #endif
    }
    
    list<VertexChain> vTop, vBottom;

    clipFirstComponent(vLeft, mid_y, closeSegments, vTopLeft, vBottomLeft);
    assert(vLeft.size() == 0);
    
    
    clipFirstComponent(vRight, mid_y, closeSegments, vTopRight, vBottomRight);
    assert(vRight.size() == 0);
}

static uint64_t VERTEX_LIMIT = 20000;   //arbitrarily guessed

void clipToLeaves(string file_base, string position, list<VertexChain>& segments, bool closeSegments, set<string> &leaves,
                   int32_t min_y = -900000000, int32_t max_y = 900000000, 
                   int32_t min_x = -1800000000, int32_t max_x = 1800000000 )
{
    //cout << position << endl;
    assert (leaves.count(position) == 0); //already stored as a leaf, i.e. a tile with full accuracy; should not happen
    
    
    uint64_t num_vertices = 0;
    for( const VertexChain &seg: segments)
        num_vertices+= seg.size();
        
    if (num_vertices <= VERTEX_LIMIT)
    {
        for( const VertexChain &seg: segments)
            writePolygonToDisk(file_base+position, seg.data(), closeSegments ); 
       
        leaves.insert(position);
        return;
    }

    int32_t mid_x = (min_x/2) + (max_x/2);  // individual division to prevent integer overflow during summation
    int32_t mid_y = (min_y/2) + (max_y/2);
    
    list<VertexChain> topLeft, topRight, bottomLeft, bottomRight;
    clipToQuadrants( segments, mid_x, mid_y, closeSegments, position != "", topLeft, topRight, bottomLeft, bottomRight);
    
    clipToLeaves(file_base, position+"0", topLeft,     closeSegments, leaves, min_y, mid_y, min_x, mid_x); topLeft.clear();
    clipToLeaves(file_base, position+"2", bottomLeft,  closeSegments, leaves, mid_y, max_y, min_x, mid_x); bottomLeft.clear();
    clipToLeaves(file_base, position+"1", topRight,    closeSegments, leaves, min_y, mid_y, mid_x, max_x); topRight.clear();
    clipToLeaves(file_base, position+"3", bottomRight, closeSegments, leaves, mid_y, max_y, mid_x, max_x); bottomRight.clear();
}

// clips the set of vertex chains recursively until they have the size of tiles of level 'level'
void clipToLevel(string file_base, string position, list<VertexChain>& segments, bool closeSegments, 
                 set<string> &leaves, uint64_t level, 
                 int32_t min_y = - 900000000, int32_t max_y =  900000000, 
                 int32_t min_x = -1800000000, int32_t max_x = 1800000000 )
{
    //cout << position << endl;
    //cout << "currently at level " << level << endl;
    if (leaves.count(position) ) return; //already stored as a leaf, i.e. a tile with full accuracy
    
    if (level == 0)    
    {
        for( const VertexChain &seg : segments)
            writePolygonToDisk(file_base+position, seg.data(), closeSegments ); 
       
        return;
    }

    int32_t mid_x = (min_x/2) + (max_x/2);  // individual division to prevent integer overflow during summation
    int32_t mid_y = (min_y/2) + (max_y/2);
    
    list<VertexChain> topLeft, topRight, bottomLeft, bottomRight;
    clipToQuadrants( segments, mid_x, mid_y, closeSegments, true, topLeft, topRight, bottomLeft, bottomRight);
    
    clipToLevel(file_base, position+"0", topLeft,     closeSegments, leaves, level - 1, min_y, mid_y, min_x, mid_x); 
    topLeft.clear();
    clipToLevel(file_base, position+"2", bottomLeft,  closeSegments, leaves, level - 1, mid_y, max_y, min_x, mid_x); 
    bottomLeft.clear();
    clipToLevel(file_base, position+"1", topRight,    closeSegments, leaves, level - 1, min_y, mid_y, mid_x, max_x); 
    topRight.clear();
    clipToLevel(file_base, position+"3", bottomRight, closeSegments, leaves, level - 1, mid_y, max_y, mid_x, max_x); 
    bottomRight.clear();
}


void clipRecursive(string file_base, string position, list<VertexChain>& segments, bool closeSegments,
                   int32_t top = -900000000, int32_t left = -1800000000, int32_t bottom = 900000000, int32_t right = 1800000000, uint32_t level = 0)
{
    uint64_t VERTEX_LIMIT = 20000;

    uint64_t num_vertices = 0;
    for ( const VertexChain &seg : segments)
        num_vertices+= seg.size();    

    for (uint32_t i = level; i; i--) cout << "  ";
    cout << "processing clipping rect '" << position << "' (" << top << ", " << left << ")-(" << bottom << ", " << right << ") with "
         << num_vertices << " vertices" << endl;
    //if (position != "" && position[0] != '3') return;
    if (num_vertices == 0) return;   //recursion termination

    //cerr << segments.size()

    if (closeSegments)  //if clipped segments are to be closed, ensure that the original segments represented a closed polygon in the first place
    {
        for ( const VertexChain seg : segments)
            assert( seg.data().front() == seg.data().back());
    }
    
    if (num_vertices < VERTEX_LIMIT)    //few enough vertices to not further subdivide the area --> write everything to disk
    {
        for ( const VertexChain seg : segments)
            writePolygonToDisk(file_base+position, seg.data(), closeSegments ); 
        return; //terminate recursion here (since no further subdivision is necessary)
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
    
    for ( const VertexChain &seg : segments)
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
        
        bool hasNonDegenerateResult = closeSegments ?
            simp.simplifyArea( (right-(uint64_t)left)/1024 ) ://also canonicalizes the result -> no need to do this again
            simp.simplifyStroke((right-(uint64_t)left)/1024);
            
        if (hasNonDegenerateResult)
            writePolygonToDisk(file_base+position, simp.data() , closeSegments);
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
        seg.clipSecondComponent( mid_h, closeSegments, vLeft, vRight);
    }
    assert(segments.size() == 0);

    if (closeSegments)
    {
        for ( const VertexChain seg: vLeft)    assert( seg.data().front() == seg.data().back());
        for ( const VertexChain seg: vRight)   assert( seg.data().front() == seg.data().back());
    }
    list<VertexChain> vTop, vBottom;

    clipFirstComponent(vLeft, mid_v, closeSegments, vTop, vBottom);
    assert(vLeft.size() == 0);
    
    clipRecursive( file_base, position+ "0", vTop,    closeSegments, top,   left, mid_v,  mid_h, level+1);
    vTop.clear();
    clipRecursive( file_base, position+ "2", vBottom, closeSegments, mid_v, left, bottom, mid_h, level+1);
    vBottom.clear();
    
    clipFirstComponent(vRight, mid_v, closeSegments, vTop, vBottom);
    assert(vRight.size() == 0);

    clipRecursive( file_base, position+ "1", vTop,    closeSegments, top,   mid_h, mid_v,  right, level+1);
    vTop.clear();
    clipRecursive( file_base, position+ "3", vBottom, closeSegments, mid_v, mid_h, bottom, right, level+1);
    vBottom.clear();
    
} 

void createTiles(string basename, list<VertexChain> &segments, bool closedPolygons)
{
    set<string> leaves;
    cout << "Creating hierarchy leaves" << endl;
    clipToLeaves(basename, "", segments, closedPolygons, leaves);
    
    uint64_t highest_depth = 0;
    for( const string &s : leaves) 
        highest_depth = max( highest_depth, s.length());
    
    cout << "[INFO] Hierarchy depth is " << highest_depth << endl;;
    assert(highest_depth < 17);
    
    double distanceThreshold = (360 * 10000000.0) / 1024;
    for (uint64_t i = 0; i < highest_depth; i++)
    {
        cout << "[INFO] Creating tiles for level " << i << " with threshold " << distanceThreshold << endl;
        list<VertexChain> copy(segments.begin(), segments.end());
        simplifyGraph(copy, distanceThreshold);
        
        clipToLevel( basename, "", copy, closedPolygons, leaves, i);
        distanceThreshold /=2;
    }
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
        
        VertexChain s= createVertexChain( way.vertices );
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

    for ( VertexChain s : lst) 
        handlePolygon("output/coast/tmp", s);
    //    poly_storage.push_back(s);
    
    createTiles( "output/seg", poly_storage, true);
    //clipRecursive( "output/coast/seg", "", poly_storage, true, -900000000, -1800000000, 900000000, 1800000000);
    
}

/* TODO let this method keep a blacklist of relations already processed through its recursion.
    Currently, relations may be processed several times (and thus cause the creation of multiple polygons),
    and the algorithm may even be caught in an infinite loop.
    */
/*void addOuterWaysToReconstructor(const OSMRelation &rel, PolygonReconstructor &recon)
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
}*/

void extractCountries()
{
/*
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
#endif*/
    Relations *relations = new Relations("intermediate/relations.idx", "intermediate/relations.data");
    Ways *ways = new Ways("intermediate/ways.idx", "intermediate/ways_int.data");

    
    for (uint64_t rel_id = 0; rel_id < relations->count(); rel_id++)
    {
        if (rel_id % 100000 == 0)
            cout << (rel_id/1000) << "k relations read." << endl;   
            
        //if (! offset[rel_id]) continue;
        OSMRelation rel = relations->get(rel_id);
        if (rel.id != rel_id) continue;
        
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
        PolygonReconstructor recon;
        
        cout << "country relation has " << rel.members.size() << " members" << endl;
        for (list<OSMRelationMember>::const_iterator way = rel.members.begin(); way != rel.members.end(); way++)
        {
            //cout << "\t member is of type " << way->type << "/" << WAY << " with role " << way->role << endl;
            if (way->type != WAY) continue;
            if (way->role != "outer") continue;
            
            //if (way->ref == 0) continue;
            //assert(way->ref < ways->cont());
            
            if (! ways->exists(way->ref)) continue;
            
            //if (way_offset[ way->ref ] == 0) continue;
            //const uint8_t * ptr = (uint8_t*)way_data_map.ptr + way_offset[ way->ref];
            OSMIntegratedWay iw = ways->get(way->ref);//(ptr , way->ref);
            VertexChain ch= createVertexChain(iw.vertices);
            recon.add(ch);
            //cout << "reconstructor has " << recon.getNumOpenEndpoints() << " open endpoints" << endl;
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
    delete ways;
    delete relations;
    clipRecursive( "output/country", "", poly_storage, true);
}

class PointQuadTreeNode
{
public:
    PointQuadTreeNode(int32_t _min_x, int32_t _max_x, int32_t _min_y, int32_t _max_y):
        tl(NULL), tr(NULL), bl(NULL), br(NULL), min_x(_min_x), max_x(_max_x), min_y(_min_y), max_y(_max_y) { };
    ~PointQuadTreeNode()
    {
        if (tl) delete tl;
        if (tr) delete tr;
        if (bl) delete bl;
        if (br) delete br;
    };
    void insert( Vertex *p);
    //void simplifyLeaves( int64_t maxAllowedDeviation);
    void clusterCloseVertices(PointQuadTreeNode *root, double distanceThreshold);
    uint64_t getNumNodes() const;
    uint64_t getNumItems() const;
    //void printHierarchy(int depth = 0);
    //void exportSegments(list<LineSegment> &segments_out);
private:
    static const unsigned int SUBDIVISION_THRESHOLD = 100; //so far just a random guess...
    static bool closerThanThreshold(Vertex a, Vertex b, double threshold);
    bool closerThanThreshold(Vertex a, double threshold);
    //static void moveCloseVertices( Vertex *ref, vector<Vertex*> &src, vector<Vertex*> &dest, double distanceThreshold);
    void extractCloseVertices(Vertex* ref, vector<Vertex*> &dest, double distanceThreshold);

    void subdivide();
    //void moveVerticesRecursively( vector<Vertex*> &target);
    
    PointQuadTreeNode        *tl, *tr, *bl, *br;
    int64_t             min_x, max_x, min_y, max_y;
    vector<Vertex*>     vertices;
};

bool PointQuadTreeNode::closerThanThreshold(Vertex a, Vertex b, double threshold)
{
    double dx = a.x-(int64_t)b.x;
    double dy = a.y-(int64_t)b.y;
    
    return dx*dx+dy*dy < threshold*threshold;
}

bool PointQuadTreeNode::closerThanThreshold(Vertex a, double threshold)
{
    //if (a.x >= min_x && a.x <= max_x && a.y >= min_y && a.y <= max_y) return true;  //'a' is inside this node
    
    return (a.x >= min_x - threshold && a.x <= max_x + threshold &&
            a.y >= min_y - threshold && a.y <= max_y + threshold);  //'a' is close enough so that a vertex in this node could be close to 'a' then 'threshold'
    
    
}

void PointQuadTreeNode::extractCloseVertices(Vertex* ref, vector<Vertex*> &dest, double distanceThreshold)
{

    if (tl || tr || bl ||br)
    {
        assert(tl && tr && bl && br);
        assert( vertices.size() == 0);

        if (tl->closerThanThreshold(*ref, distanceThreshold)) tl->extractCloseVertices(ref, dest, distanceThreshold);
        if (tr->closerThanThreshold(*ref, distanceThreshold)) tr->extractCloseVertices(ref, dest, distanceThreshold);
        if (bl->closerThanThreshold(*ref, distanceThreshold)) bl->extractCloseVertices(ref, dest, distanceThreshold);
        if (br->closerThanThreshold(*ref, distanceThreshold)) br->extractCloseVertices(ref, dest, distanceThreshold);
        return;
   } 

    //move all vertices closer than 'distanceThreshold' to 'ref', to the container 'dest'
    uint64_t idx = 0;
    while (idx < vertices.size())
    {
        assert (vertices[idx] != ref); //every reference should be present in the quad tree exactly once
        if (closerThanThreshold(*ref, *(vertices[idx]), distanceThreshold))
        {
            dest.push_back(vertices[idx]);
            vertices[idx] = vertices.back();
            vertices.pop_back();
        } else idx++;
    }
}

void PointQuadTreeNode::clusterCloseVertices(PointQuadTreeNode *root, double distanceThreshold)
{
    //#error: for whatever reason, this function does not cluster all vertices that are supposed to be clustered (i.e. that are close together)
    if (tl || tr || bl ||br)
    {
        assert(tl && tr && bl && br);
        assert( vertices.size() == 0);
        
        tl->clusterCloseVertices(root, distanceThreshold);
        tr->clusterCloseVertices(root, distanceThreshold);
        bl->clusterCloseVertices(root, distanceThreshold);
        br->clusterCloseVertices(root, distanceThreshold);
        return;
    }
    
    vector<Vertex*> cluster;
    while (vertices.size())
    {
        Vertex* v = vertices.back();
        vertices.pop_back();

        cluster.clear();
        root->extractCloseVertices(v, /*ref*/ cluster, distanceThreshold);
        
        if (cluster.size() == 0) continue;
        
        cluster.push_back(v);
        
        //cout << "merging " << cluster.size() << " vertices" << endl;
        int64_t sum_x =0;
        int64_t sum_y =0;
        
        for (uint64_t j = 0; j < cluster.size(); j++)
        {
            sum_x += cluster[j]->x;
            sum_y += cluster[j]->y;
        }
        Vertex avg( sum_x / (int64_t)cluster.size(), sum_y / (int64_t)cluster.size());
        for (uint64_t j = 0; j < cluster.size(); j++)
            *(cluster[j]) = avg;
    }
    
}

uint64_t PointQuadTreeNode::getNumNodes() const
{
    uint64_t num = 0;
    if (tl) num += 1 + tl->getNumNodes();
    if (tr) num += 1 + tr->getNumNodes();
    if (bl) num += 1 + bl->getNumNodes();
    if (br) num += 1 + br->getNumNodes();
    return num;
}

uint64_t PointQuadTreeNode::getNumItems() const
{
    uint64_t num = vertices.size();
    if (tl) num += tl->getNumItems();
    if (tr) num += tr->getNumItems();
    if (bl) num += bl->getNumItems();
    if (br) num += br->getNumItems();
    return num;
}



void PointQuadTreeNode::insert( Vertex *p)
{
    int64_t mid_x = (min_x + max_x) /2;
    int64_t mid_y = (min_y + max_y) /2;

    assert( p->x <= max_x && p->x >= min_x && p->y <= max_y && p->y >= min_y);
    if (tl || tr || bl || br)
    {
        assert (tl && tr && bl && br);
        
        if (p->x <= mid_x)
        {
            if (p->y <= mid_y) tl->insert(p);
            else               bl->insert(p);
        } else //p->x > mid_x
        {
            if (p->y <= mid_y) tr->insert(p);
            else               br->insert(p);
        }
        return;
    } else 
        vertices.push_back(p);

    
    if (vertices.size() > SUBDIVISION_THRESHOLD)
    {
        tl = new PointQuadTreeNode( min_x, mid_x, min_y, mid_y);
        tr = new PointQuadTreeNode( mid_x, max_x, min_y, mid_y);
        bl = new PointQuadTreeNode( min_x, mid_x, mid_y, max_y);
        br = new PointQuadTreeNode( mid_x, max_x, mid_y, max_y);
        
        //No need to call insert() for each vertex. The child nodes cannot have been subdivided (since we just created them)
        for( Vertex *v : vertices)
        {
            if (v->x <= mid_x)
            {
                if (v->y <= mid_y) tl->vertices.push_back(v);
                else              bl->vertices.push_back(v);
            } else //p->x > mid_x
            {
                if (v->y <= mid_y) tr->vertices.push_back(v);
                else              br->vertices.push_back(v);
            }
        }
        vertices.clear();
    }
}

bool similar(const VertexChain &a, const VertexChain &b)
{
    const vector<Vertex> &A = a.data();
    const vector<Vertex> &B = b.data();

    if (a.size() != b.size()) return false;
    for (uint64_t idx = 0; idx < a.size(); idx++)
        if (A[idx] != B[idx]) return false;

    return true;
}

void simplifyGraph(list<VertexChain> &data, double allowedDeviation)
{
    /* general approach:
     * 0. [done] merge adjacent line segments whose common vertex is shared only by them - done directly by extractHighways()
     * 1. [done] merge close-enough vertices by building an quadtree of pointers to all graph vertices
     * 2. [done] canonicalize all line segments (will remove duplicates due to merged vertices)
     * 3. [done] remove canonical line segments consisting only of a single vertex
     * 4. [done] simplify all remaining line segments with simplifyStroke()
     * 5. [done] remove duplicate edges
     */
    PointQuadTreeNode* root = new PointQuadTreeNode( -900000000, 900000000, -1800000000, 1800000000);
    //uint64_t i = 0;
    for ( VertexChain &c : data)
    {
        for( Vertex &v : c.internal_data())
        {
            //i++;
            //if (i % 1000000 == 0) cout << (i/1000000) << "M vertices registered" << endl;
            root->insert(&v);
        }
    }
    //cout << "registered " << i << " vertices" << endl;
    //cout << "created quad tree with " << root->getNumNodes() << " nodes and " << root->getNumItems() << " vertices" << endl;
    root->clusterCloseVertices(root, allowedDeviation);
    //cout << "simplified it to " << root->getNumNodes() << " nodes" << endl;
    delete root;

    // Step 2 & 3
    for ( list<VertexChain>::iterator it = data.begin(); it != data.end(); )
    {
        it->canonicalize();
        if (it->size() >= 2)    //needs to be at least a line to matter in a graph
            it++;
        else
            it = data.erase(it);
    }
    //cout << "... resulting in " << data.size() << " vertex chains after canonicalization" << endl;
    
    // Step 4
    for (list<VertexChain>::iterator c = data.begin(); c != data.end(); )
    {
        bool hasNonDegenerateResult = c->simplifyStroke( allowedDeviation);
        if (hasNonDegenerateResult)
            c++;
        else
            c = data.erase(c);
    }

    //cout << "... and " << data.size() << " vertex chains after line segment simplification" << endl;
    
    
    // Step 5
    map<Vertex, set<Vertex> > graph;
    for (list<VertexChain>::iterator c = data.begin(); c != data.end(); c++)
    {
        const vector<Vertex> &vertices = c->data();
        for (uint64_t i =0; i < c->size()-1; i++)
        {
            if (graph.count( vertices[i]   ) < 1) graph.insert( pair<Vertex, set<Vertex> >(vertices[i], set<Vertex>()));
            if (graph.count( vertices[i+1] ) < 1) graph.insert( pair<Vertex, set<Vertex> >(vertices[i], set<Vertex>()));
            
            if (graph[vertices[i]].count(vertices[i+1]) == 0) graph[vertices[i]].insert(vertices[i+1]);
            if (graph[vertices[i+1]].count(vertices[i]) == 0) graph[vertices[i+1]].insert(vertices[i]);
        }
    }
    data.clear();
    
    uint64_t numEdges = 0;
    for (map<Vertex, set<Vertex> >::const_iterator it = graph.begin(); it != graph.end(); it++)
        numEdges+= it->second.size();

    //cout << "... and " << (numEdges/2) << " vertices after duplicate removal" << endl;
    uint64_t numChains = 0;
    while (graph.size())
    {
        VertexChain c;
        Vertex v = graph.begin()->first;
        c.append(v);
        
        while (graph.count(v) )
        {
            assert( graph[v].size() > 0);
            Vertex next = *(graph[v].begin());
            
            assert( graph.count(next));
            assert( graph[next].size() > 0);
            
            graph[v   ].erase(next);
            graph[next].erase(v);
            
            if (graph[v   ].size() == 0) graph.erase(v);
            if (graph[next].size() == 0) graph.erase(next);
            c.append(next);
            v = next;
        }
        
        numChains++;
        data.push_back(c);
        //writePolygonToDisk("output/road", c.data() , false);
        
    }
    
    cout << "... aggregated into " << numChains << " final vertex chains" << endl;
    
}

void mergeAdjacentVertexChains(list<VertexChain> &chains)
{
    map<Vertex, list<VertexChain*>> endVertices;
    for (list<VertexChain>::iterator seg = chains.begin(); seg != chains.end(); seg++)
    {
        if (! endVertices.count( seg->front()))
            endVertices.insert( pair<Vertex, list<VertexChain*> >(seg->front(), list<VertexChain*>()) );

        if (! endVertices.count( seg->back()))
            endVertices.insert( pair<Vertex, list<VertexChain*> >(seg->back(), list<VertexChain*>() ) );
            
        endVertices[ seg->front()].push_back( &(*seg) );
        endVertices[ seg->back() ].push_back( &(*seg) );
    }

    
    //Next step: merge adjacent segments (if the common end point is shared *only* by them)
    
    /* Note: all VertexChain pointers are pointers into an array of VertexChains, which handles object deallocation.
             Thus, no manual 'delete' is necessary */
    for ( map<Vertex, list<VertexChain*> >::const_iterator it = endVertices.begin(); it != endVertices.end(); it++)
    {
        Vertex v = it->first;
        if (it->second.size() != 2) continue;
        
        VertexChain* c1 = it->second.front();
        VertexChain* c2 = it->second.back();
        if (c1 == c2) continue; //self-loop
        
        if (c1->back() != v) c1->reverse();
        if (c2->front()!= v) c2->reverse();
        
        assert(c1->back() == v && c2->front() == v);
        c1->append(*c2, true);
        
        /* c2's other endpoint is still registered in the endVertices map.
         * Since c2 has been merged into c1, this entry neeeds to be replaced by c1 as well*/
        assert(endVertices.count(c2->back()) > 0);
        list<VertexChain*> &c2EndList = endVertices[c2->back()];
        int num_replaced = 0;
        for (list<VertexChain*>::iterator it2 = c2EndList.begin(); it2 != c2EndList.end(); it2++)
        {
            if (*it2 == c2) 
            {   
                *it2 = c1;
                num_replaced++;
            }
        }
        assert(num_replaced == 1);
        c2->clear();
    }
    
    list<VertexChain>::iterator seg = chains.begin();
    while (seg != chains.end())
    {
        if (seg->size() == 0)
            seg = chains.erase(seg);
        else
            seg++;
    }
}
void extractAreas()
{
/*
    mmap_t idx_map = init_mmap ( "intermediate/relations.idx", true, false);
    mmap_t data_map= init_mmap ( "intermediate/relations.data",true, false);

    assert ( idx_map.size % sizeof(uint64_t) == 0);
    uint64_t num_rels = idx_map.size / sizeof(uint64_t);

    uint64_t *rel_indices = (uint64_t*)idx_map.ptr;
    uint8_t*  rel_data =    (uint8_t*)data_map.ptr;*/
  
    set<uint64_t> way_whitelist; //list of ways that were selected as part of matching relations
    Relations *relations = new Relations("intermediate/relations.idx", "intermediate/relations.data");
    
    for (uint64_t rel_id = 0; rel_id < relations->count(); rel_id++)
    {
        if (rel_id % 100000 == 0)
            cout << (rel_id/1000) << "k relations read." << endl;   

        if (! relations->exists(rel_id)) continue;
        //if (! rel_indices[rel_id]) continue;
        
        OSMRelation rel = relations->get(rel_id);//( &rel_data[rel_indices[rel_id]], rel_id);
        if ((rel["type"] != "multipolygon") && (rel["type"] != "boundary")) continue;
        if (!rel.hasKey("waterway") && !rel.hasKey("natural")) continue;
        if (rel["waterway"] != "riverbank" && rel["natural"] != "water") continue;
        
        for (const OSMRelationMember &member: rel.members)
        {
            if (member.role != "outer") continue;
            if (member.type != ELEMENT::WAY) continue;
            way_whitelist.insert( member.ref);
        }
    }
    delete relations;
    /*free_mmap(&idx_map);
    free_mmap(&data_map);*/

    /*
    mmap_t way_idx_map = init_mmap ( "ways.idx", true, false);
    mmap_t way_data_map= init_mmap ( "ways_int.data",true, false);

    assert ( way_idx_map.size % sizeof(uint64_t) == 0);
    uint64_t num_ways = way_idx_map.size / sizeof(uint64_t);

    uint64_t *way_indices = (uint64_t*)way_idx_map.ptr;
    uint8_t*  way_data =    (uint8_t*)way_data_map.ptr;*/
    Ways *ways = new Ways("ways.idx", "ways_int.data");
    PolygonReconstructor recon;

    for (uint64_t way_id = 0; way_id < ways->count(); way_id++)
    {
        if (way_id % 1000000 == 0)
            cout << (way_id/1000000) << "M ways read." << endl;   

        if (! ways->exists(way_id)) continue;
        OSMIntegratedWay way = ways->get(way_id);

        if (!way_whitelist.count(way_id))
        {
            if (!way.hasKey("waterway") && !way.hasKey("natural")) continue;
            if (way["waterway"] != "riverbank" && way["natural"] != "water") continue;
        }
        
        recon.add(createVertexChain(way.vertices));
        
        //cout << way << endl;
        
    }
    delete ways;
    /*free_mmap(&way_idx_map);
    free_mmap(&way_data_map);*/

    recon.forceClosePolygons();
    list<VertexChain> polys = recon.getClosedPolygons();
    recon.clear();
    for (list<VertexChain>::iterator it = polys.begin(); it != polys.end(); it++)
        handlePolygon("dummy", *it);


    cout << poly_storage.size() << " polygons extracted" << endl;
    clipRecursive( "output/coast/water", "", poly_storage, true);
    
}

void extractHighways()
{
    FILE *f = fopen("intermediate/roads.dump", "rb");
    int i = 0;
    int ch;
    while ( (ch = fgetc(f)) != EOF )
    {
        ungetc(ch, f);
        //int d = fputc(ch, f);
        //perror("fputc");
        //assert(d == ch);
        if (++i % 1000000 == 0) cout << (i/1000000) << "M roads read" << endl;
        OSMIntegratedWay way(f, -1);
        if (way["highway"] != "motorway") 
            if (way["highway"] != "motorway_link") 
                continue;
        
        VertexChain tmp = createVertexChain(way.vertices);
        poly_storage.push_back(tmp);
        //handlePolygon("dummy", tmp);
        //tmp.isClockwise();
        //cout << way << endl;        
    } 
    
    cout << "found " << poly_storage.size() << " highway segments" << endl;

    mergeAdjacentVertexChains(poly_storage);
    
    cout << "... of which " << poly_storage.size() << " will be present in the simplified road network" << endl;
    
//void clipToLeaves(string file_base, string position, list<VertexChain>& segments, bool closeSegments, set<string> &leaves,
    
    //set<string> leaves;
    //clipToLeaves("output/coast/road", "", poly_storage, false, leaves);
    //clipToLevel("output/coast/road", "", poly_storage, false, leaves, 2);
    createTiles("output/road", poly_storage, false);
     //simplifyGraph(poly_storage, (3600000000)/1024);
    //clipRecursive( "output/coast/road", "", poly_storage, false);
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
        if (++i % 100000 == 0) cout << (i/1000) << "k buildings/" << ftell(f)/1000000 << "MB read" << endl;
        OSMIntegratedWay way(f, -1);
        if (! (way.vertices.front() == way.vertices.back()))
           way.vertices.push_back(way.vertices.front());
        VertexChain tmp = createVertexChain(way.vertices);
        handlePolygon("dummy", tmp);
        //tmp.isClockwise();
        //cout << way << endl;        
    } 
    clipRecursive( "output/building", "", poly_storage, true);
}

int main()
{
    /*
    extractNetwork(fopen("regions.dump", "rb"), allowedDeviation, "output/regions/region");
    extractNetwork(fopen("water.dump", "rb"), allowedDeviation, "output/water/water");*/

    FILE* f = fopen("intermediate/coastline.dump", "rb");
    if (!f) { std::cout << "Cannot open file \"coastline.dump\"" << std::endl; return 0; }
    reconstructCoastline(f);//, poly_storage);

    cout << "Countries .. " << endl;
    extractCountries();

    cout << "Highways .. " << endl;
    extractHighways();

    cout << "Buildings .. " << endl;
    extractBuildings();

   
    //cout << "reconstructed a total of " << poly_storage.size() << " coastline polygons" << endl;
}
