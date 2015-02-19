
#ifndef OSM_CONSUMER_DUMPER_H
#define OSM_CONSUMER_DUMPER_H

#include <stdio.h>

#include "mem_map.h"
#include "osmConsumer.h"
#include "radixTree.h"
#include "chunkedFile.h"

class OsmConsumerDumper: public OsmBaseConsumer
{
public:
    OsmConsumerDumper();
    virtual ~OsmConsumerDumper();
protected:
    static const string base_path;

    virtual void consumeNode( OSMNode &node);
    virtual void consumeWay ( OSMWay  &way);
    virtual void consumeRelation( OsmRelation &relation); 
private:
    bool processTag(OSMKeyValuePair &tag) const;
    void filterTags(vector<OSMKeyValuePair> &tags) const;

private:
    mmap_t node_index, vertex_data, way_index, relation_index;
    ChunkedFile *nodeData, *wayData, *relationData;
    RadixTree<string> rename_key; 
    RadixTree<int> ignore_key, ignoreKeyPrefixes;    //ignore key-value pairs which are irrelevant for most applications
    uint64_t nNodes, nWays, nRelations;
    
    uint64_t node_data_synced_pos, node_index_synced_pos;
    
};

#endif

