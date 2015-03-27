
#define _FILE_OFFSET_BITS 64

#define __STDC_FORMAT_MACROS
#include <inttypes.h>   //for PRIu64
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h> //for _SC_PAGESIZE

#include <sys/mman.h>   // for madvise

#include <iostream>
#include <string>
#include <map>
#include <list>

#include "mem_map.h"
#include "osm/osmParserXml.h"
#include "consumers/osmConsumerDumper.h"
#include "osm/osmTypes.h"
#include "osm/osm_tags.h"
//#include "symbolic_tags.h"

#ifndef MUST
    #define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}
#endif

using namespace std;

#define outputBasePath "intermediate/"

static string toBucketString(string storageDirectory, uint64_t bucketId)
{
    char tmp [100];
    MUST( snprintf(tmp, 100, "%05" PRIu64, bucketId) < 100, "bucket id overflow");
    /* warning: do not attempt to integrate the following concatenation
     *          into the sprintf statement above. 'storageDirectory' has
     *          unbounded length, and thus may overflow any preallocated
     *          char array.*/
    return storageDirectory + "nodeRefs" + tmp + ".raw";
}


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

NodeBucket::NodeBucket(std::string filename)
{
    f = fopen(filename.c_str(), "wb");
    unsavedTuples = new uint64_t[2* sizeof(uint64_t) * MAX_NUM_UNSAVED_TUPLES]; //one tuple consists of two uint64_t
    numUnsavedTuples = 0;
}

NodeBucket::NodeBucket(): f(nullptr), unsavedTuples(nullptr), numUnsavedTuples(0)
{
}


NodeBucket::~NodeBucket()
{
    MUST( (f == NULL) == (unsavedTuples == NULL), "corrupted bucket");
    if (f && numUnsavedTuples)
    {
        MUST(fwrite(unsavedTuples, 2 * sizeof(uint64_t) * numUnsavedTuples, 1, f) == 1, "error writing bucket");
        delete [] unsavedTuples;
    }
}

void NodeBucket::addTuple(uint64_t wayId, uint64_t nodeId)
{
    assert( f && unsavedTuples && "bucket memory corruption");
    if (numUnsavedTuples == MAX_NUM_UNSAVED_TUPLES)
    {
        MUST(fwrite(unsavedTuples, 2* sizeof(uint64_t) * numUnsavedTuples, 1, f) == 1, "error writing bucket");
        numUnsavedTuples = 0;
    }
    
    MUST( numUnsavedTuples < MAX_NUM_UNSAVED_TUPLES, "bucket memory corruption");
    
    unsavedTuples[ 2*numUnsavedTuples  ] = wayId;
    unsavedTuples[ 2*numUnsavedTuples+1] = nodeId;
    numUnsavedTuples += 1;
}


OsmConsumerDumper::OsmConsumerDumper(std::string destinationDirectory): nNodes(0), nWays(0), nRelations(0), node_data_synced_pos(0), node_index_synced_pos(0), destinationDirectory(destinationDirectory)
{    
    MUST(this->destinationDirectory.length() > 0, "empty destination given");
    if (this->destinationDirectory.back() != '/' && this->destinationDirectory.back() != '\\')
        this->destinationDirectory += "/";

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
    truncateFile(waysIndexFilename);
    truncateFile(waysDataFilename);
    truncateFile(relationsIndexFilename);
    truncateFile(relationsDataFilename);

    vertex_data = init_mmap(verticesDataFilename.c_str()); //holds just raw vertex coordinates indexed by node_id; no tags

    node_index = init_mmap( nodesIndexFilename.c_str() );
    nodeData = new ChunkedFile(nodesDataFilename.c_str());

    way_index = init_mmap(waysIndexFilename.c_str());
    wayData = new ChunkedFile(waysDataFilename.c_str());

    relation_index = init_mmap(relationsIndexFilename.c_str());
    relationData = new ChunkedFile(relationsDataFilename.c_str());


    //FIXME: delete leftover node and way bucket files
};

OsmConsumerDumper::~OsmConsumerDumper()
{
    delete nodeData;    //close chunked file
    free_mmap(&node_index);   
   
    free_mmap(&vertex_data);
    free_mmap(&way_index);
    delete wayData;

    free_mmap(&relation_index);
    delete relationData;

    cout << "statistics: " << nNodes << " nodes, " << nWays << " ways, " << nRelations << " relations" << endl;

    /* try to delete the bucket file *after* the last one. 
     * Usually, this one should not exist, and unlink() will just fail gracefully.
     * But in case there are leftover bucket files from an earlier run, this will
     * ensure that there is no unbroken sequence of files that contains both old 
     * and new bucket files. Thus, this deletion ensures that subsequent tools do not
     * use data from earlier runs.
     */
    unlink( toBucketString(destinationDirectory, nodeRefBuckets.size()).c_str());


    for (NodeBucket* bucket : nodeRefBuckets)
        delete bucket;
}


//virtual ~OsmConsumerDumper() {};
//protected:

//pad the data file to a multiple of the page size, so that it can be opened using a memory map
/*static void padFile(FILE* file)
{
    uint64_t file_size = ftello(file);
    uint32_t page_size = sysconf(_SC_PAGESIZE);

    if (file_size % page_size != 0)
    {
        uint32_t padding = page_size - (file_size % page_size);
        uint8_t dummy = 0;
        assert(sizeof(dummy) == 1);
        while (padding--)
        fwrite(&dummy, sizeof(dummy), 1, file);
    }
    assert( ftello(file) % page_size == 0);
}*/


void OsmConsumerDumper::filterTags(vector<OSMKeyValuePair> &tags) const
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

void OsmConsumerDumper::consumeNode( OSMNode &node) 
{
    nNodes++;
    filterTags(node.tags);
    node.serializeWithIndexUpdate(*nodeData, &node_index);
    
    ensure_mmap_size( &vertex_data, (node.id+1) * 2 * sizeof(int32_t));
    int32_t* vertex_ptr = (int32_t*)vertex_data.ptr;
    vertex_ptr[2*node.id]   = node.lat;
    vertex_ptr[2*node.id+1] = node.lon;
}

static const uint64_t IS_WAY_REFERENCE = 0x8000000000000000ull;

void OsmConsumerDumper::consumeWay ( OSMWay  &way)
{
    nWays++;
    filterTags(way.tags);
    way.serialize(*wayData, &way_index);
    
    /* the following code dumps each (wayId, nodeId) tuple in a bucket file, where bucket 'i'
       stores the tuples for all nodeIds in the range [i*10M, (i+1)*10M[. These bucket files are
       later used to efficiently create reverse indices (mapping each node id to a list of way ids
       that reference said node).
       
       For a 2015 planet dump,
       the highest nodeId is about 3G, requiring about 300 buckets. Note that Linux has a soft
       ulimit at 1024 open files per process. So the bucket size needs to be adjusted
       once OSM reaches close to 10G node IDs, in order to not exceed the open file limit.
    */
    static const uint64_t NODE_BUCKET_SIZE = 10000000;
    for (const OsmGeoPosition &ref : way.refs)
    {
        uint64_t bucketNo = ref.id / NODE_BUCKET_SIZE;
        assert( bucketNo < 800 && "getting too close to the ulimit for #open files");
        
        if ( bucketNo >= nodeRefBuckets.size())
        {
            uint64_t oldNumBuckets = nodeRefBuckets.size();
            nodeRefBuckets.resize(bucketNo+1);
            
            for (uint64_t i = oldNumBuckets; i <= bucketNo; i++)
                nodeRefBuckets[i] = new NodeBucket(toBucketString(destinationDirectory, i));
        }
        nodeRefBuckets[bucketNo]->addTuple(way.id | IS_WAY_REFERENCE, ref.id);
    }
    
}

void OsmConsumerDumper::consumeRelation( OsmRelation &relation) 
{
    nRelations++;
    filterTags(relation.tags);
    relation.serializeWithIndexUpdate( *relationData, &relation_index);
    
}


