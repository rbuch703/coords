
//#include "symbolic_tags.h"

#include <stdint.h>
#include <stdlib.h> // for exit() TODO: remove once no longer needed
#include <math.h>

#include <iostream>
#include <fstream>
#include <list>
#include <set>

#include <sys/stat.h>
#include <sys/types.h>  //for mkdir

#include "osm_types.h"
#include "mem_map.h"
#include "polygonreconstructor.h"
using namespace std;

/*
uint64_t* node_index;
uint64_t* way_index;
uint64_t* relation_index;

uint8_t* osm_data;
*/
mmap_t mmap_node =          init_mmap("nodes.idx",      true, false);  //read-only memory maps
mmap_t mmap_way =           init_mmap("ways.idx",       true, false);
mmap_t mmap_relation =      init_mmap("relations.idx",  true, false);
mmap_t mmap_node_data =     init_mmap("nodes.data",     true, false);
mmap_t mmap_way_data =      init_mmap("ways_int.data",  true, false);
mmap_t mmap_relation_data = init_mmap("relations.data", true, false);

uint64_t* node_index =     (uint64_t*) mmap_node.ptr;
uint64_t* way_index =      (uint64_t*) mmap_way.ptr;
uint64_t* relation_index = (uint64_t*) mmap_relation.ptr;
uint8_t*  node_data =      (uint8_t*)  mmap_node_data.ptr;
uint8_t*  way_data =       (uint8_t*)  mmap_way_data.ptr;
uint8_t*  relation_data =  (uint8_t*)  mmap_relation_data.ptr;

static const uint64_t num_ways = mmap_way.size / sizeof(uint64_t);
static const uint64_t num_relations = mmap_relation.size / sizeof(uint64_t);
//static const uint64_t num_nodes = mmap_node.size / sizeof(uint64_t);


OSMNode             getNode(uint64_t node_id)    { return OSMNode(&node_data[node_index[node_id]], node_id);}
OSMIntegratedWay    getWay(uint64_t way_id)      { return OSMIntegratedWay(&way_data[way_index[way_id]], way_id);}
OSMRelation         getRelation(uint64_t relation_id) { return OSMRelation(&relation_data[relation_index[relation_id]], relation_id);}

map<string, uint32_t> zone_entries;

void ensureDirectoryExists(string directory)
{
    struct stat dummy;
    size_t start_pos = 0;
    
    do
    {
        size_t pos = directory.find('/', start_pos);
        string basedir = directory.substr(0, pos);  //works even if no slash is present --> pos == string::npos
        if (0!= stat(basedir.c_str(), &dummy)) //directory does not yet exist
            if (0 != mkdir(basedir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) //755
                { perror("[ERR] mkdir");}
        if (pos == string::npos) break;
        start_pos = pos+1;
    } while ( true);
}

static const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};

//obtains all "outer" ways that belong to a given relation, including those of possible sub-relations
//FIXME: blacklist those timezone relations that are part of other timezone relations
list<uint64_t> getTZWayListRecursive( uint64_t relation_id)
{
    list<uint64_t> res;
    if (! relation_index[relation_id]) {cout << "[WARN] non-existent relation " << relation_id << endl; return res; }
    OSMRelation rel = getRelation(relation_id);

    if ((rel.getValue("type") !="boundary") && (rel.getValue("type") !="multipolygon") && (rel.getValue("type") != "multilinestring"))
    {
        std::cout << "[WARN] Relation " << rel.id <<" with unknown type \"" << rel.getValue("type") << "\"" << std::endl;
        return res;
    }

    for (list<OSMRelationMember>::const_iterator it = rel.members.begin(); it != rel.members.end(); it++)
    {
        OSMRelationMember member = *it;
        if (member.type == NODE && member.role == "admin_centre") continue; //not of interest for time zones
        if (member.type == NODE && member.role == "capital") continue; 
        if (member.type == NODE && member.role == "label") continue; 
        
        if (member.type == RELATION && member.role == "region") member.role = "subarea";
        if (member.type == RELATION && member.role == "sub_area") member.role = "subarea";
        if (member.type == RELATION && member.role == "subarea")  continue;  
        
        if (member.type == WAY && member.role == "exclave") member.role = "inner"; //same as "inner"
        if (member.type == WAY && member.role == "inner") continue; //specifies a hole in a polygon FIXME: handle these
        if (member.type == RELATION && member.role == "inner") continue; //DITO
        
        if (member.type == RELATION && (member.role == "outer" || member.role == ""))
        {
            list<uint64_t> tmp = getTZWayListRecursive(member.ref);
            res.insert(res.end(), tmp.begin(), tmp.end());
            continue;
        }
        
        if (member.type == WAY && member.role =="enclave") member.role = "outer";   // those are all supposed to
        if (member.type == WAY && member.role =="") member.role = "outer";          // mean the same in OSM
        
        if ((member.type != WAY) || (member.role != "outer"))
        {
            std::cout << "Unexpected Member (" << ELEMENT_NAMES[member.type] << " " << member.ref 
                      << ") with role '" << member.role << "' in rel " << rel.id << endl; 
           continue;
       }
       res.push_back(member.ref);
    }
    return res;
}

/*
PolygonSegment createPolygonSegment(uint64_t way_id)
{
    
    OSMIntegratedWay way = getWay(way_id);

    PolygonSegment seg;
    for (list<Vertex>::const_iterator vertex = way.vertices.begin(); vertex != way.vertices.end(); vertex++)
    {
        //if (! node_index[*node_id]) {cout << "[WARN] non-existent node " << *node_id << ", skipping" << endl; continue;}
        seg.append(*vertex);
    }
    return seg;

}*/

void dumpPolygon(string file_base, const PolygonSegment& segment)
{
    size_t pos = file_base.rfind('/');
    string directory = file_base.substr(0, pos);
    //zone = zone.substr(pos+1);
    ensureDirectoryExists(directory);
    
    int file_num = 1;
    if ( zone_entries.count(file_base) )
        file_num = ++zone_entries[file_base];
    else (zone_entries.insert( pair<string, uint32_t>(file_base, 1)));
    
    ofstream out;
    char tmp[200];
    snprintf(tmp, 200, "%s_%u.csv", file_base.c_str(), file_num);
    //string filename = zone+"_"+ file_num+".csv";
    out.open(tmp);
    for (list<Vertex>::const_iterator vertex = segment.vertices().begin(); vertex != segment.vertices().end(); vertex++)
    {
        out << vertex->x << ", " << vertex->y << endl;
    }
    out.close();
}

void getSubRelations( uint64_t relation_id, set<uint64_t> &outSubRelations)
{
    if (! relation_index[relation_id]) {cout << "[WARN] non-existent relation " << relation_id << endl; return; }
    OSMRelation rel = getRelation(relation_id);

    for (list<OSMRelationMember>::const_iterator it = rel.members.begin(); it != rel.members.end(); it++)
    {
        if (it->type == RELATION)
        {
            outSubRelations.insert(it->ref);
            getSubRelations( it->ref, outSubRelations);
        }
    }         
}

string timezone_basedir = "timezones";
/*
void extractTimeZonesRelations()
{
    std::cout << "Scanning " << num_relations/1000 << "k relations" << std::endl;
    list<PolygonSegment> polygons;
    int numTimezoneRelations = 0;
    //relations that are part of timezone-relations and not by themselves timezones (even if they are tagged as such)
    set<uint64_t> timezoneSubRelations; 
    for (uint64_t i = 0; i < num_relations; i++)
    {
        if (! relation_index[i]) continue;
        OSMRelation rel = getRelation(i);
        if (!rel.hasKey("timezone")) continue;
        numTimezoneRelations++;
        getSubRelations(rel.id, timezoneSubRelations);
    }
    
    //timezoneSubRelations.clear(); //FIXME: cleared blacklist for debugging
    cout << numTimezoneRelations << " timezone relations, " << timezoneSubRelations.size() << " black-listed sub-relations." << endl;
    PolygonReconstructor recon;
    for (uint64_t i = 0; i < num_relations; i++)
    
    //for (uint64_t i = 120027; i == 120027; i++)
    //for (uint64_t i = 153557; i == 153557; i++)
    {
        if (! relation_index[i]) continue;
        if (timezoneSubRelations.count(i)) continue; // if a parent relation is already a time zone
        
        OSMRelation rel = getRelation(i);
        if (!rel.hasKey("timezone")) continue;
        list<uint64_t> ways = getTZWayListRecursive(i);
        cout << "Timezone " << rel.getValue("timezone") << "(relation " << rel.id << ") with " << ways.size() << " ways" << endl;
        
        for (list<uint64_t>::const_iterator way_id = ways.begin(); way_id != ways.end(); way_id++)
        {
            recon.add(createPolygonSegment(*way_id));
        }
        recon.forceClosePolygons();
        //list<PolygonSegment> polygons = createPolygons(ways);
        for (list<PolygonSegment>::const_iterator polygon = recon.getClosedPolygons().begin(); 
                polygon != recon.getClosedPolygons().end(); polygon++)
            dumpPolygon( timezone_basedir+"/"+"all", *polygon);
            //dumpPolygon( timezone_basedir+"/"+rel.getValue("timezone"), *polygon);
        recon.clear();
    }
}*/




void extractCoastline()
{
    double allowedDeviation = 100* 40000.0; // about 40km (~1/1000th of the earth circumference)

    PolygonReconstructor recon;
    cout << "Generating coastline polygons" << endl;
    //list<uint64_t> coastline_ways;
    for (uint64_t i = 0; i < num_ways; i++)
    {
        if (i % 1000000 == 0) std::cout << i/1000000 << "M ways scanned, " << std::endl;
        if (! way_index[i]) continue;
        OSMIntegratedWay way = getWay(i);
        
        if (way.getValue("natural") == "coastline")
        {
            //cout << "coastline segment " << i << endl;
            PolygonSegment s =way.toPolygonSegment();
            PolygonSegment *poly = recon.add(s);
            if (poly) // if a new closed polygon has been created
            {
                //int nVertices = seg->getVerticesDEBUG().size();
                if (poly->simplifyArea(allowedDeviation))
                {
                    //cout << "Simplified: " << seg->getVerticesDEBUG().size() << "/" << nVertices << endl;
                    dumpPolygon("coastline/seg",*poly);
                }
                recon.clearPolygonList();
            }
        }
    }

    assert( recon.getClosedPolygons().size() == 0);
    //recon.clear();
    recon.forceClosePolygons();
    cout << "Done simplifying proper polygons; now processing 'unclean' ones" << endl;
    int nUnclean = 0;
    for (list<PolygonSegment>::const_iterator seg =recon.getClosedPolygons().begin(); seg != recon.getClosedPolygons().end(); seg++)
    {
        nUnclean++;
        PolygonSegment poly = *seg;
        //int nVertices = poly.getVerticesDEBUG().size();
        if (poly.simplifyArea(allowedDeviation))
        {
            //cout << "Simplified: " << poly.getVerticesDEBUG().size() << "/" << nVertices << endl;
            dumpPolygon("coastline/seg",poly);
        }
        
    }
    cout << "Found " << zone_entries["coastline/seg"] << " coastline ways, " << nUnclean << " unclean" << endl;
    
}

void extractBorders()
{
    double allowedDeviation = 100* 40000.0; // about 40km (~1/1000th of the earth circumference)
    
    map<Vertex, list<PolygonSegment*> > borders;
    set<PolygonSegment*> border_storage;
    
    //list<Way> loops;
    for (uint64_t i = 0; i < num_ways; i++)
    {
        if ((i) % 1000000 == 0) std::cout << i/1000000 << "M ways scanned, " << std::endl;
        if (! way_index[i]) continue;
        OSMIntegratedWay way = getWay(i);
        
        if (way["boundary"] == "administrative" && way["admin_level"] == "2")
        {
            PolygonSegment s = way.toPolygonSegment();
            if (s.front() == s.back()) { 
                if (s.simplifyArea(allowedDeviation))
                    dumpPolygon("borders/border",s);
                continue; 
            }
            
            PolygonSegment *seg = new PolygonSegment(s);
            
            borders[seg->front()].push_back(seg);
            borders[seg->back()].push_back (seg);
            border_storage.insert(seg);
        }
    }
    // copy of endpoint vertices from 'borders', so that we can iterate over the endpoints and still safely modify the map
    list<Vertex> endpoints; 
    for (map<Vertex, list<PolygonSegment*> >::const_iterator v = borders.begin(); v != borders.end(); v++)
        endpoints.push_back(v->first);
    
    cout << "Found " << border_storage.size() << " border segments" << endl;
    for (list<Vertex>::const_iterator it = endpoints.begin(); it!= endpoints.end(); it++)
    {
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
                if (seg1->simplifyArea(allowedDeviation))
                    dumpPolygon("borders/border",*seg1);
                border_storage.erase(seg1);
                delete seg1;
                continue; 
            }
            borders[seg1->front()].push_back(seg1);
            borders[seg1->back() ].push_back(seg1);
        }
    }
    
    cout << "Merged them to " << border_storage.size() << " border segments" << endl;
    for (set<PolygonSegment*>::iterator it = border_storage.begin(); it != border_storage.end(); it++)
    {
        PolygonSegment *seg = *it;
        seg->simplifyStroke(allowedDeviation);
        dumpPolygon("borders/border", *seg);
        delete seg;
    }
    
    cout << "Altogether " << zone_entries["borders/border"] << " country boundary segments (including loops)" << endl;
    
}

/*
void extractTimeZonesWays()
{
    std::cout << "Scanning " << num_ways/1000000 << "M ways" << std::endl;
    for (uint64_t i = 0; i < num_ways; i++)
    {
        if (i % 10000000 == 0) cout << (i/1000000) << "M ways processed" << std::endl;
        if (! way_index[i]) continue;
        OSMWay way = getWay(i);
        if (!way.hasKey("timezone")) continue;
        
        string zone = way.getValue("timezone");
        size_t pos = zone.find('/');
        if (pos == string::npos)
        {
            cout << "Malformed timezone name '" << zone << "', skipping" << endl;
            continue;
        }
        
        dumpPolygon(timezone_basedir + "/" + zone, createPolygonSegment( way.id));
        
    }
}*/

int main()
{
    assert( mmap_node.size && mmap_way.size && mmap_relation.size && 
            mmap_node_data.size && mmap_way_data.size && mmap_relation_data.size &&
            "Empty data file(s)");

    extractBorders();
    //extractCoastline();
    
    
    //extractTimeZonesRelations();
    //extractTimeZonesWays();
    /*
    for (uint64_t i = 0; i < num_relations; i++)
    //for (uint64_t i = 153557; i == 153557; i++)
    {
        if (! relation_index[i]) continue;
        OSMRelation rel = getRelation(i);
        if (rel.hasKey("name") && rel.getValue("name").find("Jefferson County") !=string::npos &&
            rel.hasKey("admin_level") && rel.getValue("admin_level") == "6")
            //rel.hasKey("nist:state_fips") && rel.getValue("nist:state_fips") == "21")
            
            //std::cout << rel << std::endl << endl;
    }*/
    
    //cout << getRelation(11980) << endl;
    //return 0;


    
    //getTZWayListRecursive(79981);
    
    //cout << getRelation(16240) << endl;
    //cout << getRelation(365331) << endl;
    //cout << getRelation(179296) << endl;
    //cout << getRelation(82675) << endl;   //part of 79981, why?
    
 
    //std::cout << "Found " << coastline_ways.size() << " coastline ways" << endl;
    
    
    free_mmap(&mmap_node);
    free_mmap(&mmap_way);
    free_mmap(&mmap_relation);
    free_mmap(&mmap_node_data);
    free_mmap(&mmap_way_data);
    free_mmap(&mmap_relation_data);
}

