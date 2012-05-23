
#include "osm_types.h"
#include "mem_map.h"

#include <stdint.h>
#include <stdlib.h> // for exit() TODO: remove once no longer needed
#include <math.h>

#include <iostream>
#include <fstream>
#include <list>
#include <set>

#include <sys/stat.h>
#include <sys/types.h>  //for mkdir

using namespace std;

/*
uint64_t* node_index;
uint64_t* way_index;
uint64_t* relation_index;

uint8_t* osm_data;
*/
mmap_t mmap_node = init_mmap("node.idx", true, false);  //read-only memory map
mmap_t mmap_way = init_mmap("way.idx", true, false);
mmap_t mmap_relation = init_mmap("relation.idx", true, false);
mmap_t mmap_data = init_mmap("osm.db", true, false);

uint64_t* node_index =     (uint64_t*) mmap_node.ptr;
uint64_t* way_index =      (uint64_t*) mmap_way.ptr;
uint64_t* relation_index = (uint64_t*) mmap_relation.ptr;
uint8_t*  osm_data =       (uint8_t*)  mmap_data.ptr;

static const uint64_t num_ways = mmap_way.size / sizeof(uint64_t);
static const uint64_t num_relations = mmap_relation.size / sizeof(uint64_t);


OSMNode     getNode(uint64_t node_id)    { return OSMNode(&osm_data[node_index[node_id]], node_id);}
OSMWay      getWay(uint64_t way_id)      { return OSMWay(&osm_data[way_index[way_id]], way_id);}
OSMRelation getRelation(uint64_t relation_id) { return OSMRelation(&osm_data[relation_index[relation_id]], relation_id);}

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

PolygonSegment createPolygonSegment(uint64_t way_id)
{
    
    OSMWay way = getWay(way_id);

    PolygonSegment seg;
    for (list<uint64_t>::const_iterator node_id = way.refs.begin(); node_id != way.refs.end(); node_id++)
    {
        if (! node_index[*node_id]) {cout << "[WARN] non-existent node " << *node_id << ", skipping" << endl; continue;}
        seg.append(getNode(*node_id).toVertex());
    }
    return seg;

}


class PolygonReconstructor
{
public:
    ~PolygonReconstructor() {clear();} 
    
    /** adds a PolygonSegment to the Reconstructor state. 
      * if this addition results in the generation of a new closed polygon, a pointer to said polygon is returned
      * otherwise, NULL is returned
      * Returns true 
    */
    PolygonSegment* add(const PolygonSegment &s)
    {
//        if (! way_index[*way_ref]) 
//            { cout << "[WARN] non-existent way " << *way_ref << ", skipping" << endl; continue; }
    
        PolygonSegment *seg = new PolygonSegment(s);

          //if there is another segment that connects seemlessly  to this one
        while ( openEndPoints.count(seg->front()) || openEndPoints.count(seg->back()))
        {
            if (openEndPoints.count(seg->front())) //other polygon connects to 'front' of this one
            {
                PolygonSegment *other = openEndPoints[seg->front()];
                openEndPoints.erase( other->front());
                openEndPoints.erase( other->back());
                
                if (other->front() == seg->front()) 
                {
                    seg->reverse(); //have to fit front-to-back
                    assert( seg->back() == other->front() );
                    seg->append(*other);
                } else
                {
                    assert( other->back() == seg->front() );
                    other->append(*seg);
                    *seg = *other;
                }
                delete other;
            }
            
            if (openEndPoints.count(seg->back())) //other polygon connects to 'back' of this one
            {
                PolygonSegment *other = openEndPoints[seg->back()];
                openEndPoints.erase( other->front());
                openEndPoints.erase( other->back());
                
                if (other->back() == seg->back()) 
                {
                    seg->reverse();
                    assert( other->back() == seg->front());
                    other->append(*seg);
                    *seg = *other;
                } else
                {
                    assert( seg->back() == other->front() );
                    seg->append(*other);
                }
                delete other;
            }
        } 

        if ( seg->front() == seg->back())   //if the segment itself is a closed polygon
        { 
            res.push_back(*seg); 
            delete seg; 
            return &res.back();
        } else
        {
            assert( seg->front() != seg->back() );
            
            assert ( openEndPoints.count(seg->front()) == 0);
            assert ( openEndPoints.count(seg->back()) == 0 );
            openEndPoints[seg->front()] = seg;
            openEndPoints[seg->back()] = seg;
        }
        return false;
    }
    
    /** uses heuristics to create polygons from the PolygonSegments in openEndPoints, even if
        the segments do not connect directly to each other */
    void forceClosePolygons()
    {
        /** at this point, all polygons that can be closed have been closed.
          * or the remaining segments, we attempt to find segments that are as close together as possible*/
        uint32_t nEndPoints = openEndPoints.size();
        Vertex vEndPoints[nEndPoints];
        uint32_t i = 0;
        for (map<Vertex, PolygonSegment*>::const_iterator poly = openEndPoints.begin(); poly != openEndPoints.end(); poly++, i++)
            vEndPoints[i] = poly->first;
            
        while (nEndPoints)
        {
            uint64_t minDist = 0xFFFFFFFFFFFFFFFFll;
            int min_i=1, min_j=0;
            for (i = 1; i < nEndPoints; i++)
                for (uint32_t j = 0; j < i; j++)
                {
                    uint64_t dist = vEndPoints[i].squaredDistanceTo( vEndPoints[j]);
                    if (dist < minDist) { minDist = dist; min_i = i; min_j = j;}
                }
            assert(minDist != 0xFFFFFFFFFFFFFFFFll);
            cout << "Minimum gap size is " << (sqrt(minDist)/100) << "m" << endl;
            assert( openEndPoints.count(vEndPoints[min_i]) && openEndPoints.count(vEndPoints[min_j]) );
            PolygonSegment* seg1 = openEndPoints[vEndPoints[min_i]];
            PolygonSegment* seg2 = openEndPoints[vEndPoints[min_j]];
            
            Vertex v1 = vEndPoints[min_i];
            Vertex v2 = vEndPoints[min_j];

            i = 0;
            while (i < nEndPoints)
            {
                if (vEndPoints[i] == seg1->front() || vEndPoints[i] == seg1->back() ||
                    vEndPoints[i] == seg2->front() || vEndPoints[i] == seg2->back()   )
                    {
                        --nEndPoints;
                        vEndPoints[i] = vEndPoints[nEndPoints];
                    }
                else i++;
            }
            
            if (seg1 == seg2) // same polygon segment --> close the loop
            {
                cout << "closing gap of  " 
                     << sqrt( seg1->back().squaredDistanceTo(seg1->front()))/100.0 << "m"<< endl;
            
                openEndPoints.erase( seg1->front());
                openEndPoints.erase( seg1->back());
                seg1->append(seg1->front()); // to really make it closed
                res.push_back(*seg1);
                delete seg1;
            } else
            {
                openEndPoints.erase( seg1->front());
                openEndPoints.erase( seg1->back());
                openEndPoints.erase( seg2->front());
                openEndPoints.erase( seg2->back());
                    
                if      (v1 == seg1->front() && v2 == seg2->front()) { seg1->reverse(); seg1->append(*seg2, false); }
                else if (v1 == seg1->back()  && v2 == seg2->front()) {                  seg1->append(*seg2, false); }
                else if (v1 == seg1->front() && v2 == seg2->back() ) { seg2->append(*seg1, false); *seg1 = *seg2;   }
                else if (v1 == seg1->back()  && v2 == seg2->back() ) { seg2->reverse(); seg1->append(*seg2, false); }
                else assert(false);
                
                delete seg2;
                assert ( openEndPoints.count(seg1->front()) == 0);
                assert ( openEndPoints.count(seg1->back() ) == 0);
                openEndPoints[seg1->front()] = seg1;
                openEndPoints[seg1->back()]  = seg1;
                vEndPoints[nEndPoints++] = seg1->front();
                vEndPoints[nEndPoints++] = seg1->back();

            }    
        }
        assert(openEndPoints.size() == 0);
    }
    
    void clear() {
        set<PolygonSegment*> segs;
        /** we need to delete all remaining manually allocated polygon segments
          * These are all registered in openEndPoints, but each polygon segment appears twice in this list.
          * So, we first have to obtain a list of unique pointers to these segments*/
        for (map<Vertex, PolygonSegment*>::iterator it = openEndPoints.begin(); it != openEndPoints.end(); it++)
            if (!segs.count(it->second)) segs.insert(it->second);
         //delete remaining PolygonSegment

        for (set<PolygonSegment*>::iterator seg = segs.begin(); seg != segs.end(); seg++)
            delete *seg;
            
        openEndPoints.clear();
        res.clear();
            
    }
    void clearPolygonList() { res.clear();}
    const list<PolygonSegment>& getClosedPolygons() const {return res;}

private:
    map<Vertex, PolygonSegment*> openEndPoints;
    list<PolygonSegment> res;
    
};


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
        OSMWay way = getWay(i);
        
        if (way.getValue("natural") == "coastline")
        {
            //cout << "coastline segment " << i << endl;
            PolygonSegment s =createPolygonSegment(i);
            PolygonSegment *poly = recon.add(s);
            if (poly) // if a new closed polygon has been created
            {
                //int nVertices = seg->getVerticesDEBUG().size();
                if (poly->simplify(allowedDeviation))
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
        if (poly.simplify(allowedDeviation))
        {
            //cout << "Simplified: " << poly.getVerticesDEBUG().size() << "/" << nVertices << endl;
            dumpPolygon("coastline/seg",poly);
        }
        
    }
    cout << "Found " << zone_entries["coastline/seg"] << " coastline ways, " << nUnclean << " unclean" << endl;
    
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
    extractCoastline();
    //TODO: store all unsimplified coastlines in a single binary file for fast later processing
    
    
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
    free_mmap(&mmap_data);
}

