
#include "osm_types.h"
#include "mem_map.h"

#include <stdint.h>
#include <iostream>

using namespace std;

uint64_t* node_index;
uint64_t* way_index;
uint64_t* relation_index;

uint8_t* osm_data;

OSMNode     getNode(uint64_t node_id)    { return OSMNode(&osm_data[node_index[node_id]], node_id);}
OSMWay      getWay(uint64_t way_id)      { return OSMWay(&osm_data[way_index[way_id]], way_id);}
OSMRelation getRelation(uint64_t relation_id) { return OSMRelation(&osm_data[relation_index[relation_id]], relation_id);}

int main()
{
    //mmap_t mmap_node = init_mmap("node.idx", true, false);  //read-only memory map
    mmap_t mmap_way = init_mmap("way.idx", true, false);
    //mmap_t mmap_relation = init_mmap("relation.idx", true, false);
    mmap_t mmap_data = init_mmap("osm.db", true, false);

    //node_index =     (uint64_t*) mmap_node.ptr;
    way_index =      (uint64_t*) mmap_way.ptr;
    //relation_index = (uint64_t*) mmap_relation.ptr;
    osm_data =       (uint8_t*)  mmap_data.ptr;

    //uint64_t num_relations = mmap_relation.size / sizeof(uint64_t);
    uint64_t num_ways = mmap_way.size / sizeof(uint64_t);

    for (uint64_t i = 0; i < num_ways; i++)
    {
        if (i % 1000000 == 0) std::cout << i/1000000 << "M ways scanned" << std::endl;
        if (! way_index[i]) continue;
        OSMWay way = getWay(i);
        if (way.hasKey("timezone"))
            std::cout << way.getValue("timezone") << std::endl;
    }
    
    /*
    for (uint64_t i = 0; i < num_relations; i++)
    {
        if (! relation_index[i]) continue;
        OSMRelation rel = getRelation(i);
        if (rel.hasKey("timezone"))
            std::cout << rel.getValue("timezone") << std::endl;
    } */   
    //free_mmap(&mmap_node);
    free_mmap(&mmap_way);
    //free_mmap(&mmap_relation);
    free_mmap(&mmap_data);
}

