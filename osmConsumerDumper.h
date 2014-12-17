
#ifndef OSM_CONSUMER_DUMPER_H
#define OSM_CONSUMER_DUMPER_H

#include <stdio.h>

#include "mem_map.h"
#include "osmConsumer.h"
#include "radixTree.h"

class OsmConsumerDumper: public OsmBaseConsumer
{
public:
    OsmConsumerDumper();
protected:
    static const string base_path;

    virtual void onAllWaysConsumed ();
    virtual void onAllNodesConsumed ();
    virtual void onAllRelationsConsumed ();

    virtual void consumeNode( OSMNode &node);
    virtual void consumeWay ( OSMWay  &way);
    virtual void consumeRelation( OSMRelation &relation); 
private:
    bool processTag(OSMKeyValuePair &tag) const;
    void filterTags(vector<OSMKeyValuePair> &tags) const;

private:
    mmap_t node_index, vertex_data, way_index, relation_index;
    FILE *node_data, *way_data, *relation_data;
    //FILE *building_data, *highway_data, *landuse_data, *natural_data;
    //map<OSMKeyValuePair, uint8_t> symbolic_tags;
    RadixTree<string> rename_key; 
    RadixTree<int> ignore_key, ignoreKeyPrefixes;    //ignore key-value pairs which are irrelevant for a viewer application
    uint64_t nNodes, nWays, nRelations;
    
    uint64_t node_data_synced_pos, node_index_synced_pos;
    
};

#endif

