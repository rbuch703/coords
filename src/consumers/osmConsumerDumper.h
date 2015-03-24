
#ifndef OSM_CONSUMER_DUMPER_H
#define OSM_CONSUMER_DUMPER_H

#include <stdio.h>

#include "mem_map.h"
#include "consumers/osmConsumer.h"
#include "containers/radixTree.h"
#include "containers/chunkedFile.h"

/* FIXME: it is likely that using this caching mechanism does not significantly speed up the algorithm,
          but just make the code more confusing.
   TODO: benchmark performance of using NodeBucket vs. directly writing to FILE*; revert to FILE* code
         (removing NodeBucket altogether) if the difference in execution time is not significant.
*/
struct NodeBucket {
    NodeBucket(std::string filename);
    NodeBucket();
    ~NodeBucket();
    FILE* f;
    uint64_t *unsavedTuples;
    uint64_t numUnsavedTuples;
        
    void addTuple(uint64_t wayId, uint64_t nodeId);
private:
    static const uint64_t MAX_NUM_UNSAVED_TUPLES = 100000;

};

class OsmConsumerDumper: public OsmBaseConsumer
{
public:
    OsmConsumerDumper(std::string destinationDirectory);
    virtual ~OsmConsumerDumper();
protected:
    static const string base_path;

    virtual void consumeNode( OSMNode &node);
    virtual void consumeWay ( OSMWay  &way);
    virtual void consumeRelation( OsmRelation &relation); 
private:
    void filterTags(vector<OSMKeyValuePair> &tags) const;

private:
    mmap_t node_index, vertex_data, way_index, relation_index;
    ChunkedFile *nodeData, *wayData, *relationData;
    //RadixTree<string> rename_key; 
    RadixTree<int> ignore_key, ignoreKeyPrefixes;    //ignore key-value pairs which are irrelevant for most applications
    uint64_t nNodes, nWays, nRelations;
    
    uint64_t node_data_synced_pos, node_index_synced_pos;
    std::string     nodesDataFilename,     nodesIndexFilename, verticesDataFilename;
    std::string      waysDataFilename,      waysIndexFilename;
    std::string relationsDataFilename, relationsIndexFilename;
    std::string destinationDirectory;
    std::vector<NodeBucket*> nodeRefBuckets;
};

#endif

