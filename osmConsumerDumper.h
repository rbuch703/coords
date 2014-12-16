
#ifndef OSM_CONSUMER_DUMPER_H
#define OSM_CONSUMER_DUMPER_H

#include <set>

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
    void processTags(list<OSMKeyValuePair> &tags);

private:
    mmap_t node_index, vertex_data, way_index/*, way_int_index*/, relation_index;
    FILE *node_data, *way_data, *way_int_data, *relation_data;
    FILE *building_data, *highway_data, *landuse_data, *natural_data;
    //map<OSMKeyValuePair, uint8_t> symbolic_tags;
    map<string, string> rename_key; 
    set<string> ignore_key;    //ignore key-value pairs which are irrelevant for a viewer application
    uint64_t nNodes, nWays, nRelations;
    
    uint64_t node_data_synced_pos, node_index_synced_pos;
    
};

#endif

