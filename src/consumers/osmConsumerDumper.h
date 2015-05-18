
#ifndef OSM_CONSUMER_DUMPER_H
#define OSM_CONSUMER_DUMPER_H

#include <stdio.h>
#include <string>
#include <vector>

#include "misc/mem_map.h"
#include "consumers/osmConsumer.h"
#include "containers/radixTree.h"
#include "containers/chunkedFile.h"
#include "containers/bucketFileSet.h"

class OsmConsumerDumper: public OsmBaseConsumer
{
public:
    OsmConsumerDumper(std::string destinationDirectory);
    virtual ~OsmConsumerDumper();
protected:
    static const std::string base_path;

    virtual void consumeNode( OsmNode &node);
    virtual void consumeWay ( OsmWay  &way);
    virtual void consumeRelation( OsmRelation &relation); 
private:
    void filterTags(std::vector<OsmKeyValuePair> &tags) const;

private:
    mmap_t node_index, vertex_data, way_index, relation_index;
    ChunkedFile *nodeData, *wayData, *relationData;
    RadixTree<int> ignore_key, ignoreKeyPrefixes;    //ignore key-value pairs that are irrelevant for most applications
    uint64_t nNodes, nWays, nRelations;
    uint64_t node_data_synced_pos, node_index_synced_pos;
    std::string     nodesDataFilename,     nodesIndexFilename, verticesDataFilename;
    std::string      waysDataFilename,      waysIndexFilename;
    std::string relationsDataFilename, relationsIndexFilename;
    std::string destinationDirectory;
    BucketFileSet<uint64_t> nodeRefBuckets;
};

#endif

