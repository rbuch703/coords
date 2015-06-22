
#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <string>
#include <map>
#include <list>

#include "config.h"
#include "consumers/osmConsumerDumper.h"
#include "containers/chunkedFile.h"
#include "osm/osmParserXml.h"
#include "osm/osmTypes.h"
#include "osm/osm_tags.h"

using namespace std;

//#define outputBasePath "intermediate/"


static void truncateFile(string filename) {
    FILE* f= fopen(filename.c_str() , "wb+"); 
    if (f == NULL) 
    {
        std::cerr << "error: cannot create/modify file '" << filename << "': ";
        perror("");
        exit(EXIT_FAILURE);
    } 
    fclose(f);
}

OsmConsumerDumper::OsmConsumerDumper(std::string destinationDirectory): 
    nNodes(0), nWays(0), nRelations(0), node_data_synced_pos(0), node_index_synced_pos(0), 
    destinationDirectory( (destinationDirectory.back() == '/' || destinationDirectory.back() == '\\' ) ? destinationDirectory : destinationDirectory + "/"), 
    nodeRefBuckets ( this->destinationDirectory + "nodeRefs", BUCKET_SIZE, false),
    wayBuckets( this->destinationDirectory+"ways", NODES_OF_WAYS_BUCKET_SIZE, false)
{    

    nodesDataFilename      = this->destinationDirectory + "nodes.data";
    nodesIndexFilename     = this->destinationDirectory + "nodes.idx";
    verticesDataFilename   = this->destinationDirectory + "vertices.data";
    waysDataFilename       = this->destinationDirectory + "ways.data";
    waysIndexFilename      = this->destinationDirectory + "ways.idx";
    relationsDataFilename  = this->destinationDirectory + "relations.data";
    relationsIndexFilename = this->destinationDirectory + "relations.idx";

    for (uint32_t i = 0; i < num_ignore_keys; i++)
        ignore_key.insert(ignore_keys[i], 0);

    for (uint32_t i = 0; i < num_ignore_key_prefixes; i++)
        ignoreKeyPrefixes.insert(ignore_key_prefixes[i], 0);
        
    truncateFile(nodesIndexFilename);
    truncateFile(nodesDataFilename);
    truncateFile(verticesDataFilename);
    //truncateFile(waysIndexFilename);
    //truncateFile(waysDataFilename);
    truncateFile(relationsIndexFilename);
    truncateFile(relationsDataFilename);

    vertex_data = init_mmap(verticesDataFilename.c_str()); //holds just raw vertex coordinates indexed by node_id; no tags

    node_index = init_mmap( nodesIndexFilename.c_str() );
    nodeData = new ChunkedFile(nodesDataFilename.c_str());

    //way_index = init_mmap(waysIndexFilename.c_str());
    //wayData = new ChunkedFile(waysDataFilename.c_str());

    relation_index = init_mmap(relationsIndexFilename.c_str());
    relationData = new ChunkedFile(relationsDataFilename.c_str());
};

OsmConsumerDumper::~OsmConsumerDumper()
{
    delete nodeData;    //close chunked file
    free_mmap(&node_index);   
   
    free_mmap(&vertex_data);
    //free_mmap(&way_index);
    //delete wayData;

    free_mmap(&relation_index);
    delete relationData;

    cout << "statistics: " << nNodes << " nodes, " << nWays << " ways, " << nRelations << " relations" << endl;

}


void OsmConsumerDumper::filterTags(vector<OsmKeyValuePair> &tags) const
{
    for (uint64_t i = 0; i < tags.size();)
    {
        if (ignore_key.at(tags[i].first.c_str()) || ignoreKeyPrefixes.containsPrefixOf( tags[i].first.c_str()) )
        {
            tags[i] = tags[tags.size()-1];
            tags.pop_back();
        }
        else
            i++;
    }   
}

void OsmConsumerDumper::consumeNode( OsmNode &node) 
{
    nNodes++;
    filterTags(node.tags);
    node.serialize(*nodeData, &node_index, &vertex_data);
}

void OsmConsumerDumper::consumeWay ( OsmWay  &way)
{
    nWays++;
    filterTags(way.tags);
    way.serialize( wayBuckets.getFile(way.id));
    
    /* the following code dumps each (wayId, nodeId) tuple in a bucket file, where bucket 'i'
     * stores the tuples for all nodeIds in the range [i*10M, (i+1)*10M[. These bucket files are
     * later used to efficiently create reverse indices (mapping each node id to a list of way
     * ids that reference said node).
     *
     * For a 2015 planet dump,
     * the highest nodeId is about 3G, requiring about 300 buckets. Note that Linux has a soft
     * ulimit at 1024 open files per process. So the bucket size needs to be adjusted
     * once OSM reaches close to 10G node IDs, in order to not exceed the open file limit.
    */
    
    for (const OsmGeoPosition &ref : way.refs)
        nodeRefBuckets.write(ref.id, way.id | IS_WAY_REFERENCE);
}

void OsmConsumerDumper::consumeRelation( OsmRelation &relation) 
{
    nRelations++;
    filterTags(relation.tags);
    relation.serializeWithIndexUpdate( *relationData, &relation_index);
    
}


