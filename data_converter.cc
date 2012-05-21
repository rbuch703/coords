
#include "osm_types.h"
#include "mem_map.h"

#include <stdint.h>
#include <stdlib.h> // for exit() TODO: remove once no longer needed
#include <math.h>

#include <iostream>
#include <fstream>

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

string timezone_basedir = "timezones";

void ensureDirectoryExists(string directory)
{
    struct stat dummy;
    if (0!= stat(directory.c_str(), &dummy)) //directory does not yet exist
        if (0 != mkdir(directory.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) //755
            { perror("[ERR] mkdir");}
}

static const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};

//obtains all "outer" ways that belong to a given relation, including those of possible sub-relations
list<OSMRelationMember> getTZWayListRecursive( uint64_t relation_id)
{
    list<OSMRelationMember> res;
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
            list<OSMRelationMember> tmp = getTZWayListRecursive(member.ref);
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
       res.push_back(member);
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


/** takes a list of OSMWays and connects these to form closed polygons*/
list<PolygonSegment> createPolygons(list<OSMRelationMember> &ways)
{
    list<PolygonSegment> res;
    map<Vertex, PolygonSegment*> openEndPoints;
    for (list<OSMRelationMember>::const_iterator way = ways.begin(); way != ways.end(); way++)
    {
        if (! way_index[way->ref]) 
            { cout << "[WARN] non-existent way " << way->ref << ", skipping" << endl; continue; }
    
        PolygonSegment *seg = new PolygonSegment(createPolygonSegment(way->ref));

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
                    *seg = *seg + *other;
                } else
                {
                    assert( other->back() == seg->front() );
                    *seg = *other + *seg;
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
                    *seg = *other + *seg;
                } else
                {
                    assert( seg->back() == other->front() );
                    *seg = *seg + *other;
                }
                delete other;
            }
        } 

        if ( seg->front() == seg->back())   //if the segment itself is a closed polygon
        { 
            res.push_back(*seg); 
            delete seg; 
            continue;
        } else
        {
            assert( seg->front() != seg->back() );
            
            assert ( openEndPoints.count(seg->front()) == 0);
            assert ( openEndPoints.count(seg->back()) == 0 );
            openEndPoints[seg->front()] = seg;
            openEndPoints[seg->back()] = seg;
        }
    }
    
    
    /** at this point, all polygons that can be closed have been closed.
      * or the remaining segments, we attempt to find segments that are as close together as possible*/
    uint32_t nEndPoints = openEndPoints.size();
    Vertex vEndPoints[nEndPoints];
    uint32_t i = 0;
    for (map<Vertex, PolygonSegment*>::const_iterator poly = openEndPoints.begin(); poly != openEndPoints.end(); poly++, i++)
        vEndPoints[i] = poly->first;
        
    uint64_t minDist = 0xFFFFFFFFFFFFFFFFll;
    int min_i=1, min_j=0;
    while (nEndPoints)
    {
        for (i = 1; i < nEndPoints; i++)
            for (uint32_t j = 0; j < i; j++)
            {
                uint64_t dist = vEndPoints[i].distanceFrom( vEndPoints[j]);
                if (dist < minDist) { minDist = dist; min_i = i; min_j = j;}
            }
        assert(minDist != 0xFFFFFFFFFFFFFFFFll);
        cout << "Minimum gap size is " << (sqrt(minDist)/100) << "m" << endl;
        assert( openEndPoints.count(vEndPoints[min_i]) && openEndPoints.count(vEndPoints[min_j]) );
        PolygonSegment* seg1 = openEndPoints[vEndPoints[min_i]];
        PolygonSegment* seg2 = openEndPoints[vEndPoints[min_j]];
        vEndPoints[min_i] = vEndPoints[--nEndPoints];   //is a noop if min_i was the last element, but is still a valid op;
        /** The element that has just been moved cannot be min_j, because min_j is guaranteed to be smaller than min_i*/
        vEndPoints[min_j] = vEndPoints[--nEndPoints];
        
        if (seg1 == seg2) // same polygon segment --> close the loop
        {
            openEndPoints.erase( seg1->front());
            openEndPoints.erase( seg1->back());
            seg1->append(seg1->back()); // to really make it closed
            res.push_back(*seg1);
            delete seg1;
        } else
        {
            //TODO: vEndPoints still contains the other end points of seg1 and seg2. Make sure their references in
            //      openEndPoints point to the merged segments
            break;
            assert(false && "Not Implemented");
        }
    }
    
    
    cout << res.size() << " closed polygons, " << (openEndPoints.size()) << " open polygon segments" << endl;
    static int num = 0;
    
    for (map<Vertex, PolygonSegment*>::const_iterator poly = openEndPoints.begin(); poly != openEndPoints.end(); poly++)
    {
        num++;
        char c_num[200];
        snprintf(c_num, 200, "%d", num);
        ofstream f( (string("tmp/poly_") + c_num + ".csv").c_str());
        f << *poly->second;
    }
    
    
    
    assert(openEndPoints.size() == 0);
    return res;
}

void dumpPolygon(string zone, const PolygonSegment& segment)
{
    size_t pos = zone.find('/');
    string directory = zone.substr(0, pos);
    zone = timezone_basedir+"/"+zone;
    //zone = zone.substr(pos+1);
    ensureDirectoryExists(timezone_basedir+"/"+directory);
    
    int file_num = 1;
    if ( zone_entries.count(zone) )
        file_num = ++zone_entries[zone];
    else (zone_entries.insert( pair<string, uint32_t>(zone, 1)));
    
    ofstream out;
    char tmp[200];
    snprintf(tmp, 200, "%s_%u.csv", zone.c_str(), file_num);
    //string filename = zone+"_"+ file_num+".csv";
    out.open(tmp);
    for (list<Vertex>::const_iterator vertex = segment.vertices().begin(); vertex != segment.vertices().end(); vertex++)
    {
        out << vertex->x << ", " << vertex->y << endl;
    }
    out.close();
}

void extractTimeZonesRelations()
{
    std::cout << "Scanning " << num_relations/1000 << "k relations" << std::endl;
    list<PolygonSegment> polygons;
    //for (uint64_t i = 0; i < num_relations; i++)
    for (uint64_t i = 120027; i == 120027; i++)
    {
        if (! relation_index[i]) continue;
        OSMRelation rel = getRelation(i);
        if (!rel.hasKey("timezone")) continue;
        
        list<OSMRelationMember> ways = getTZWayListRecursive(i);
        cout << "Timezone " << rel.getValue("timezone") << "(relation " << rel.id << ") with " << ways.size() << " ways" << endl;
        list<PolygonSegment> polygons = createPolygons(ways);
        for (list<PolygonSegment>::const_iterator polygon = polygons.begin(); polygon != polygons.end(); polygon++)
            dumpPolygon( rel.getValue("timezone"), *polygon);
        
        //break; //FIXME: debug break
        /*for (list<OSMRelationMember>::const_iterator it = rel.members.begin(); it!= rel.members.end(); it++)
            std::cout << it->role << std::endl;*/
        //std::cout << rel << std::endl;
    }
}



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
        
        dumpPolygon(zone, createPolygonSegment( way.id));
        
    }
}

int main()
{
    ensureDirectoryExists(timezone_basedir);    
    extractTimeZonesRelations();
    //extractTimeZonesWays();
    //getTZWayListRecursive(79981);
    
    //cout << getRelation(16240) << endl;
    //cout << getRelation(365331) << endl;
    //cout << getRelation(179296) << endl;
    //cout << getRelation(82675) << endl;   //part of 79981, why?
    
    /*
    for (uint64_t i = 0; i < num_ways; i++)
    {
        if (i % 1000000 == 0) std::cout << i/1000000 << "M ways scanned" << std::endl;
        if (! way_index[i]) continue;
        OSMWay way = getWay(i);
        if (way.hasKey("timezone"))
            std::cout << way.getValue("timezone") << std::endl;
    }*/
    
    
    free_mmap(&mmap_node);
    free_mmap(&mmap_way);
    free_mmap(&mmap_relation);
    free_mmap(&mmap_data);
}

