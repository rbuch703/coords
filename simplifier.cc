
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

/**
  *
  */
void clipRecursive(string file_base, string position, list<VertexChain>& segments, bool closeSegments,
                   int32_t top = -900000000, int32_t left = -1800000000, int32_t bottom = 900000000, int32_t right = 1800000000, uint32_t level = 0)
{
    uint64_t VERTEX_LIMIT = 20000;

    uint64_t num_vertices = 0;
    BOOST_FOREACH( const VertexChain &seg, segments)
        num_vertices+= seg.size();    

    for (uint32_t i = level; i; i--) cout << "  ";
    cout << "processing clipping rect '" << position << "' (" << top << ", " << left << ")-(" << bottom << ", " << right << ") with "
         << num_vertices << " vertices" << endl;
    //if (position != "" && position[0] != '3') return;
    if (num_vertices == 0) return;   //recursion termination

    //cerr << segments.size()

    if (closeSegments)  //if clipped segments are to be closed, ensure that the original segments represented a closed polygon in the first place
    {
        BOOST_FOREACH( const VertexChain seg, segments)
            assert( seg.data().front() == seg.data().back());
    }
    
    if (num_vertices < VERTEX_LIMIT)    //few enough vertices to not further subdivide the area --> write everything to disk
    {
        BOOST_FOREACH( const VertexChain seg, segments)
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
        BOOST_FOREACH( const VertexChain seg, vLeft)    assert( seg.data().front() == seg.data().back());
        BOOST_FOREACH( const VertexChain seg, vRight)   assert( seg.data().front() == seg.data().back());
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
    
    clipRecursive( "output/coast/seg", "", poly_storage, true, -900000000, -1800000000, 900000000, 1800000000);
    
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
    clipRecursive( "output/coast/country", "", poly_storage, true);
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
    void simplifyLeaves( int64_t maxAllowedDeviation);
    uint64_t getNumNodes() const;
    //void printHierarchy(int depth = 0);
    //void exportSegments(list<LineSegment> &segments_out);
private:
    static const unsigned int SUBDIVISION_THRESHOLD = 100; //so far just a random guess...
    //bool addToQuadrant(LineSegment seg);    
    //bool intersectedByRecursive( LineSegment edge1, list<LineSegment> &createdSegments, int depth);
    //int  coversQuadrants( LineSegment edge, bool &tl, bool &tr, bool &bl, bool &br) const;
    void subdivide();
    void moveVerticesRecursively( vector<Vertex*> &target);
    
    PointQuadTreeNode        *tl, *tr, *bl, *br;
    int64_t             min_x, max_x, min_y, max_y;
    vector<Vertex*>     vertices;
    //AABoundingBox       box;
    //BigInt            mid_x, mid_y;
};

uint64_t PointQuadTreeNode::getNumNodes() const
{
    uint64_t num = 0;
    if (tl) num += 1 + tl->getNumNodes();
    if (tr) num += 1 + tr->getNumNodes();
    if (bl) num += 1 + bl->getNumNodes();
    if (br) num += 1 + br->getNumNodes();
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
    } else 
        vertices.push_back(p);

    
    if (vertices.size() > SUBDIVISION_THRESHOLD)
    {
        tl = new PointQuadTreeNode( min_x, mid_x, min_y, mid_y);
        tr = new PointQuadTreeNode( mid_x, max_x, min_y, mid_y);
        bl = new PointQuadTreeNode( min_x, mid_x, mid_y, max_y);
        br = new PointQuadTreeNode( mid_x, max_x, mid_y, max_y);
        
        //No need to call insert() for each vertex. The child nodes cannot have been subdivided (since we just created them)
        BOOST_FOREACH( Vertex *v, vertices)
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

void PointQuadTreeNode::simplifyLeaves( int64_t allowedDeviation)
{
    double dx = (max_x-min_x);
    double dy = (max_y-min_y);
    double diag_sq = (dx*dx+dy*dy);
    if (diag_sq > allowedDeviation*allowedDeviation) //still above threshold
    {
        if (tl) tl->simplifyLeaves(allowedDeviation);
        if (tr) tr->simplifyLeaves(allowedDeviation);
        if (bl) bl->simplifyLeaves(allowedDeviation);
        if (br) br->simplifyLeaves(allowedDeviation);
        return;
    }

    vector<Vertex*> verts;
    
    moveVerticesRecursively(verts);
    assert(vertices.size() == 0);
    
    if (verts.size() == 0) return;
    //cout << "merging " << verts.size() << " vertices" << endl;
    int64_t sum_x = 0;
    int64_t sum_y = 0;
    BOOST_FOREACH(Vertex* v, verts)
    {
        sum_x += v->x;
        sum_y += v->y;
    }
    
    //the casting here is important. Without it, the division would be performed on unsigned operands
    Vertex avg( sum_x / (int64_t)verts.size(), sum_y / (int64_t)verts.size() );
    assert(avg.x >= -900000000 && avg.x <= 900000000 && avg.y >= -1800000000 && avg.y <= 1800000000);
    
    BOOST_FOREACH(Vertex* v, verts)
        *v = avg;    
}

void PointQuadTreeNode::moveVerticesRecursively( vector<Vertex*> &target)
{
    if (tl) { tl->moveVerticesRecursively( target ); delete tl; tl = NULL;}
    if (tr) { tr->moveVerticesRecursively( target ); delete tr; tr = NULL;}
    if (bl) { bl->moveVerticesRecursively( target ); delete bl; bl = NULL;}
    if (br) { br->moveVerticesRecursively( target ); delete br; br = NULL;}
    target.insert(target.end(), vertices.begin(), vertices.end() );
    vertices.clear();
}


void simplifyGraph(list<VertexChain> &data, double allowedDeviation)
{
#warning CONTINUEHERE
    /* general approach:
     * 1. merge close-enough vertices by building an quadtree of pointers to all graph vertices
     *    1a. [done] merge vertices in leaf nodes if whole leaf is smaller than allowDeviation
     *    1b. in bigger leaves, merge individual groups of vertices
     *    1c. for vertices close to tree node boundary, also merge vertices of adjacent nodes
     * 2. canonicalize all line segments (will remove duplicates due to merged vertices)
     * 3. remove canonical line segments consisting only of a single vertex
     * 4. merge adjacent line segments whose common vertex is shared only by them
     * 6. merge line segments that share both endpoints and where each vertex of one segment is close enough to the other segment (and vice-versa)
     * 5. simplify all remaining line segments with simplifyStroke()
     */
    PointQuadTreeNode* root = new PointQuadTreeNode( -900000000, 900000000, -1800000000, 1800000000);
    uint64_t i = 0;
    BOOST_FOREACH( VertexChain &c, data)
    {
        BOOST_FOREACH( Vertex &v, c.internal_data())
        {
            i++;
            if (i % 1000000 == 0) cout << (i/1000000) << "M vertices registered" << endl;
            root->insert(&v);
        }
    }
    cout << "created quad tree with " << root->getNumNodes() << " nodes" << endl;
    root->simplifyLeaves(allowedDeviation);
    cout << "simplified it to " << root->getNumNodes() << " nodes" << endl;
    delete root;

    for ( list<VertexChain>::iterator it = data.begin(); it != data.end(); )
    {
        it->canonicalize();
        if (it->size() >= 2)    //needs to be at least a line to matter in a graph
            it++;
        else
            it = data.erase(it);
    }
    cout << "... resulting in " << data.size() << " vertex chains after canonicalization" << endl;

    BOOST_FOREACH( VertexChain &c, data)
    {
        bool hasNonDegenerateResult = c.simplifyStroke(1800000000/1024);
        if (hasNonDegenerateResult)
            writePolygonToDisk("output/road", c.data() , false);
    }
    
    
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
        
        VertexChain tmp(way.vertices);
        poly_storage.push_back(tmp);
        //handlePolygon("dummy", tmp);
        //tmp.isClockwise();
        //cout << way << endl;        
    } 
    
    cout << "found " << poly_storage.size() << " highway segments" << endl;
    map<Vertex, list<VertexChain*>> endVertices;
    for (list<VertexChain>::iterator seg = poly_storage.begin(); seg != poly_storage.end(); seg++)
    {
        if (! endVertices.count( seg->front()))
            endVertices.insert( pair<Vertex, list<VertexChain*> >(seg->front(), list<VertexChain*>()) );

        if (! endVertices.count( seg->back()))
            endVertices.insert( pair<Vertex, list<VertexChain*> >(seg->back(), list<VertexChain*>() ) );
            
        // the algorithm should work even if the segment forms a loop
        // assert(seg->front() != seg.back());
        endVertices[ seg->front()].push_back( &(*seg) );
        endVertices[ seg->back() ].push_back( &(*seg) );
    }

    
    //Next step: merge adjacent segments (if the common end point is shared *only* by them)
    
    /* Note: all VertexChain pointers are pointers into an array of VertexChain, which handles object deallocation.
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
    
    list<VertexChain>::iterator seg = poly_storage.begin();
    while (seg != poly_storage.end())
    {
        if (seg->size() == 0)
            seg = poly_storage.erase(seg);
        else
            seg++;
    }
    
    cout << "... of which " << poly_storage.size() << " will be present in the simplified road network" << endl;
    
    simplifyGraph(poly_storage, (3600000000)/1024);
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
        if (++i % 100000 == 0) cout << (i/1000) << "k buildings read" << endl;
        OSMIntegratedWay way(f, -1);
        if (! (way.vertices.front() == way.vertices.back()))
           way.vertices.push_back(way.vertices.front());
        VertexChain tmp(way.vertices);
        handlePolygon("dummy", tmp);
        //tmp.isClockwise();
        //cout << way << endl;        
    } 
    clipRecursive( "output/coast/building", "", poly_storage, true);
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
    //extractCountries();
    extractHighways();
    //extractBuildings();
    //extractGermany();
/*
    FILE* f = fopen("coastline.dump", "rb");
    if (!f) { std::cout << "Cannot open file \"coastline.dump\"" << std::endl; return 0; }
    reconstructCoastline(f);//, poly_storage);
    */
    //cout << "reconstructed a total of " << poly_storage.size() << " coastline polygons" << endl;
}
