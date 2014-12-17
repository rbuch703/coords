
//#include "symbolic_tags.h"

#include <stdint.h>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <list>
#include <set>

#include "osmTypes.h"
//#include "mem_map.h"
#include <helpers.h>
//#include "polygonreconstructor.h"
using namespace std;

FILE* fWayIdx =       fopen("intermediate/ways.idx", "rb");
FILE* fWayData =      fopen("intermediate/ways_int.data", "rb");
FILE* fRelationIdx =  fopen("intermediate/relations.idx", "rb");
FILE* fRelationData = fopen("intermediate/relations.data", "rb");
uint64_t numRelations, numWays;

typedef pair<list<OSMKeyValuePair>, FILE*> OSMFilterConfig;

//OSMNode             getNode(uint64_t node_id)    { return OSMNode(&node_data[node_index[node_id]], node_id);}
#if 0
OSMIntegratedWay getWay(uint64_t way_id)      
{

    OSMIntegratedWay way(fWayIdx, fWayData, way_id);
    /*if (way_index[way_id]) 
    {
        const uint8_t *data_ptr = &way_data[way_index[way_id]];
        return OSMIntegratedWay(data_ptr, way_id);
    }*/

    if (way.id != way_id)
        cerr << "[WARN] trying to access not-existing way " << way_id << ", skipping" << endl;
        
    return way;
}*/


OSMRelation getRelation(uint64_t relation_id) 
{ 
    OSMRelation res(fRelationIdx, fRelationData, relation_id);
    
    if (res.id != relation_id)
        cerr << "[WARN] trying to access not-existing relation " << relation_id << ", skipping" << endl;
        
    return res;
}
#endif


void getSubRelations( uint64_t relation_id, set<uint64_t> &outSubRelations)
{
    OSMRelation rel(fRelationIdx, fRelationData, relation_id);
    if (rel.id != relation_id)
        {cout << "[WARN] non-existent relation " << relation_id << endl; return; }


    for (list<OSMRelationMember>::const_iterator it = rel.members.begin(); it != rel.members.end(); it++)
    {
        if ((it->type == RELATION) && (! outSubRelations.count(it->ref)))
        {
            outSubRelations.insert(it->ref);
            getSubRelations( it->ref, outSubRelations);
        }
    }         
}

void getOutlineWaysRecursive(const OSMRelation &relation, set<uint64_t> &ways_out, set<uint64_t>&relationsChecked )
{
    if ( relationsChecked.count(relation.id)) 
        return;
    
    relationsChecked.insert( relation.id);
    for (list<OSMRelationMember>::const_iterator member = relation.members.begin(); member != relation.members.end(); member++)
    {
        if (member->type == WAY) ways_out.insert(member->ref);
        else if (member->type == RELATION)
        {
            if ((member->role == "outer" || member->role == "exclave" || member->role == "")  && !ways_out.count(member->ref))
                getOutlineWaysRecursive( OSMRelation(fRelationIdx, fRelationData, member->ref), ways_out, relationsChecked);
        }
    }
}



void dumpWays( list<OSMFilterConfig> config)
{

    map<FILE*, set<uint64_t> > ways_from_relations;

    /** 
        scan all relations for the tags from 'extract_config' and record their ways.
        This is done because a way (e.g. a coastline) may either be tagged correctly,
        or may not be tagged at all but is part of a relation that is tagged correctly,
        or both! Thus, we have to make sure that each way matching the criteria is 
        read and dumped exactly once, independent of whether it is tagged, or belongs 
        to a tagged relation.
        To ensure this, for each tag list corresponding to a certain set of data (e.g. all country
        borders), we first record the IDs of all ways that are part of relations 
        matching these tags. Later, we scan all ways for these tags as well, dump their 
        data to a file, and *remove* their IDs from the ID lists we create here.
        As a result, these ID lists should contain all those ways that are part of 
        relations meeting the tag criteria, but who themselves do not. Finally, those
        ways have to be read and dumped to file as well.
      */
    cout << "First phase, scanning relations" << endl;
    for (uint64_t i = 0; i < numRelations; i++)
    {
        /*cout << i << endl;
        if ( (i+1) % 100000 == 0) 
            exit(0);*/
            
        if ((i+1) % 1000000 == 0) std::cout << (i+1)/1000000 << "M relations scanned, " << std::endl;

        //std::cout << i << endl;
        OSMRelation rel(fRelationIdx, fRelationData, i);
        if (rel.id != i) 
            continue;

        //std::cout << rel << endl;
                    
        //cout << rel << endl;
        for (const OSMFilterConfig &cfg : config)
        //for (list<pair<list<OSMKeyValuePair>, FILE* > >::const_iterator it = config.begin(); it != config.end(); it++)
        {
            
            const list<OSMKeyValuePair> &tags = cfg.first;
            FILE* file = cfg.second;
            bool matches = true;
            for (const OSMKeyValuePair &tag : tags)
            //for (list<OSMKeyValuePair>::const_iterator tag = tags.begin(); tag != tags.end() && matches; tag++)
            {
                matches &= rel.hasKey(tag.first);
                matches &= (tag.second == "*" || rel[tag.first] != tag.second); // value= "*" --> match any value
            }
            if (matches)
            {
                set<uint64_t> tmp;
                getOutlineWaysRecursive(rel, ways_from_relations[file],  tmp);
            }
        }
        
    }
    
    for ( map<FILE*, set<uint64_t> >::const_iterator it = ways_from_relations.begin(); it!= ways_from_relations.end(); it++)
    {
        cout << it->second.size() << " ways from relations" << endl;
    }
    
    cout << "Second phase, scanning ways" << endl;
    /** second phase, dump all ways matching the tag criteria, but remove 
        them from the relations lists*/    
    //list<Way> loops;
    for (uint64_t i = 0; i < numWays; i++)
    {
        if ((i) % 1000000 == 0) std::cout << i/1000000 << "M ways scanned, " << std::endl;
        OSMIntegratedWay way(fWayIdx, fWayData, i);
        if (way.id != i) continue;

        for (list<pair<list<OSMKeyValuePair>, FILE* > >::const_iterator it = config.begin(); it != config.end(); it++)
        {
            const list<OSMKeyValuePair> &tags = it->first;
            FILE* file = it->second;
            bool matches = true;
            for (list<OSMKeyValuePair>::const_iterator tag = tags.begin(); tag != tags.end() && matches; tag++)
            {
                if (! way.hasKey(tag->first)) matches = false;
                // value= "*" --> match any value
                if ((tag->second != "*") && (way[tag->first] != tag->second))
                    matches= false;
            }
            if (matches)
            {
                way.serialize(file);
                ways_from_relations[file].erase( way.id);
            }
        }
    }
    
    /** third phase, read the remaining ways (those that did not match the 
        criteria by themselves) from the relations lists, and dump their 
        contents to the files*/

    cout << "Third phase, reading non-duplicate ways from relations" << endl;

    for ( map<FILE*, set<uint64_t> >::const_iterator it = ways_from_relations.begin(); it!= ways_from_relations.end(); it++)
    {
        FILE* file = it->first;
        const set<uint64_t> &ids = it->second;
        cout << ids.size() << " ways from relations" << endl;
        for ( set<uint64_t>::const_iterator it = ids.begin(); it != ids.end(); it++)
            OSMIntegratedWay(fWayIdx, fWayData, *it).serialize(file);
    }

}


int main()
{
#if 0
    assert( /*mmap_node.size && */mmap_way.size && mmap_relation.size && 
            /*mmap_node_data.size &&*/ mmap_way_data.size && mmap_relation_data.size &&
            "Empty data file(s)");
#endif

    //double allowedDeviation = 100* 40000.0; // about 40km (~1/1000th of the earth circumference)
    fseek(fRelationIdx, 0, SEEK_END);
    numRelations = ftell(fRelationIdx);
    assert(numRelations % 8 == 0);  //contains only uint64_t values
    numRelations /= 8;
    
    fseek(fWayIdx, 0, SEEK_END);
    numWays = ftell(fWayIdx);
    assert(numWays % 8 == 0);  //contains only uint64_t values
    numWays /= 8;
    

    
    
    list<pair<list<OSMKeyValuePair>, FILE*> > extract_config;
    
    const char* base_dir = "intermediate";
    ensureDirectoryExists(base_dir);
    
    list<OSMKeyValuePair> country_border_tags;
    country_border_tags.push_back( OSMKeyValuePair("boundary", "administrative"));
    country_border_tags.push_back( OSMKeyValuePair("admin_level", "2"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>( country_border_tags, fopen("intermediate/countries.dump", "wb")));

    list<OSMKeyValuePair> regions;
    regions.push_back( OSMKeyValuePair("boundary", "administrative"));
    regions.push_back( OSMKeyValuePair("admin_level", "4"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>( regions, fopen("intermediate/regions.dump", "wb")));

    list<OSMKeyValuePair> cities;
    cities.push_back( OSMKeyValuePair("boundary", "administrative"));
    cities.push_back( OSMKeyValuePair("admin_level", "8"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>( cities, fopen("intermediate/cities.dump", "wb")));
    
    list<OSMKeyValuePair> roads;
    roads.push_back( OSMKeyValuePair("highway", "*"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>( roads, fopen("intermediate/roads.dump", "wb")));

    list<OSMKeyValuePair> water;
    water.push_back( OSMKeyValuePair("natural", "water"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>( water, fopen("intermediate/water.dump", "wb")));
    
    list<OSMKeyValuePair> buildings;
    buildings.push_back( OSMKeyValuePair("building", "*"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>(buildings, fopen("intermediate/buildings.dump", "wb")));
    
    list<OSMKeyValuePair> coastline;
    coastline.push_back( OSMKeyValuePair("natural", "coastline"));
    extract_config.push_back( pair<list<OSMKeyValuePair>, FILE*>( coastline, fopen("intermediate/coastline.dump", "wb")));

    dumpWays(extract_config);
    
    for( const OSMFilterConfig &cfg : extract_config)
        fclose(cfg.second);
    
    //free_mmap(&mmap_node);
    //free_mmap(&mmap_way);
    //free_mmap(&mmap_relation);
    //free_mmap(&mmap_node_data);
    //free_mmap(&mmap_way_data);
    //free_mmap(&mmap_relation_data);
}

