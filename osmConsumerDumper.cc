
#define _FILE_OFFSET_BITS 64

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
#include "osmParserXml.h"
#include "osmConsumerDumper.h"
#include "osmTypes.h"
#include "osm_tags.h"
//#include "symbolic_tags.h"

#ifndef MUST
    #define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}
#endif

using namespace std;

#define outputBasePath "intermediate/"

const char* nodes_data_filename =       outputBasePath "nodes.data";
const char* nodes_index_filename=       outputBasePath "nodes.idx";
const char* vertices_data_filename =    outputBasePath "vertices.data";
const char* ways_data_filename =        outputBasePath "ways.data";
const char* ways_index_filename=        outputBasePath "ways.idx";
const char* relations_data_filename=    outputBasePath "relations.data";
const char* relations_index_filename=   outputBasePath "relations.idx";

static void truncateFile(string filename) {
    FILE* f= fopen(filename.c_str() , "wb+"); 
    if (f == NULL) {perror("[ERR] fopen:"); assert(false);} 
    fclose(f);
}


OsmConsumerDumper::OsmConsumerDumper(): nNodes(0), nWays(0), nRelations(0), node_data_synced_pos(0), node_index_synced_pos(0)
{    

    //for situations where several annotation keys exist with the same meaning
    //this dictionary maps them to a common unique key
    /*rename_key.insert( "postal_code", "addr:postcode");
    rename_key.insert( "url", "website"); //according to OSM specs, "url" is obsolete
    rename_key.insert( "phone", "contact:phone");
    rename_key.insert( "fax", "contact:fax");
    rename_key.insert( "email", "contact:email");
    rename_key.insert( "addr:housenumber", "addr:streetnumber");*/
    //TODO: replace natural=lake by natural=water
    for (uint32_t i = 0; i < num_ignore_keys; i++)
        ignore_key.insert(ignore_keys[i], 0);

    for (uint32_t i = 0; i < num_ignore_key_prefixes; i++)
        ignoreKeyPrefixes.insert(ignore_key_prefixes[i], 0);
        
    truncateFile(nodes_index_filename);
    truncateFile(nodes_data_filename);
    truncateFile(vertices_data_filename);
    truncateFile(ways_index_filename);
    truncateFile(ways_data_filename);
//    truncateFile(ways_int_data_filename);
    truncateFile(relations_index_filename);
    truncateFile(relations_data_filename);

    node_index = init_mmap( nodes_index_filename );
    
    nodeData = new ChunkedFile(nodes_data_filename);
    /*node_data = fopen(nodes_data_filename, "wb+");
    const char* node_magic = "ON10"; //OSM Nodes v. 1.0
    fwrite(node_magic, 4, 1, node_data);*/

    vertex_data = init_mmap(vertices_data_filename); //holds just raw vertex coordinates indexed by node_id; no tags

};

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


void OsmConsumerDumper::onAllNodesConsumed () {
    //cout << "===============================================" << endl;
    //cout << "writing mmaped contents to disk" << endl;
    //padFile(node_data);
    
    /* to clear the caches, we have to:
     * - force Linux to flush out the dirty areas of the data files using sync_file_range
     * - advise Linux that all flushed file areas can be evicted from the page cache 
     *   (that would not have worked on dirty pages).
    */
    
    /*sync_file_range( fileno(node_data), 0, 0, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
    posix_fadvise( fileno(node_data), 0, 0, POSIX_FADV_DONTNEED);
    fclose( node_data ); //don't need node data any more*/
    delete nodeData;    //close chunked file
    
    sync_file_range( node_index.fd, 0, 0, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
    madvise(node_index.ptr, node_index.size, MADV_DONTNEED);
    free_mmap(&node_index);
    
    //sync_file_range( vertex_data.fd, 0, 0, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
    //madvise(vertex_data.ptr, vertex_data.size, MADV_DONTNEED);
    free_mmap(&vertex_data);
    //re-open the vertex mmap read-only. This should improve Linux' mapping behavior for the next phase,
    //where ways are read and written sequentially, but vertices are read randomly
    vertex_data = init_mmap(vertices_data_filename, true, false); 
    
    //cout << "== Done parsing Nodes ==" << endl;

    //setup output for ways
    way_index = init_mmap(ways_index_filename);
    wayData = new ChunkedFile(ways_data_filename);
};


void OsmConsumerDumper::onAllWaysConsumed () {
    free_mmap(&vertex_data);
    free_mmap(&way_index);
    //free_mmap(&way_int_index);
    
    //padFile(way_data);
    //fclose(way_data);
    delete wayData;
    
//    padFile(way_int_data);
//    fclose(way_int_data);
    
    //cout << "== Done parsing Ways ==" << endl;

    //setup output for relations
    relation_index = init_mmap(relations_index_filename);
    
    relationData = new ChunkedFile(relations_data_filename);
    /*relation_data = fopen(relations_data_filename, "wb+");
    const char* relation_magic = "OR10"; //OSM Relation v. 1.0
    fwrite(relation_magic, 4, 1, relation_data);*/

};   


void OsmConsumerDumper::onAllRelationsConsumed () {
    free_mmap(&relation_index);
    delete relationData;
        
    //cout << "==================== done =======================" << endl;
    cout << "statistics: " << nNodes << " nodes, " << nWays << " ways, " << nRelations << " relations" << endl;
    
}; 


/* modifies the 'tag' to reflect the rename rules. 
 * @Returns: whether the tag is to be kept (true) or discarded (false) */
 
bool OsmConsumerDumper::processTag(OSMKeyValuePair &tag) const
{
    //const string* renameValue = rename_key.at(tag.first.c_str());
    //if ( renameValue ) tag.first = *renameValue;
        
    if ( ignore_key.at(tag.first.c_str()) )
        return false;
        
    if ( ignoreKeyPrefixes.containsPrefixOf( tag.first.c_str()))
        return false;

    return true;    
}

void OsmConsumerDumper::filterTags(vector<OSMKeyValuePair> &tags) const
{
    for (uint64_t i = 0; i < tags.size();)
    {
        if (processTag(tags[i]) )
            i++;
        else
        {
            tags[i] = tags[tags.size()-1];
            tags.pop_back();
        }   
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

/*    
    if (nNodes % 1000000 == 0)
    {

        cout << endl << "\e[1A";
        cout << "processed " << (nNodes / 1000000) << "M nodes; writing data to disk ...";
        cout << " (" << ((ftell(node_data) - node_data_synced_pos) / 1000000) << "MB node data)";
        //sync_file_range( fileno(node_data), node_data_synced_pos, 0, SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE|SYNC_FILE_RANGE_WAIT_AFTER);
        //posix_fadvise( fileno(node_data), node_data_synced_pos, 0, POSIX_FADV_DONTNEED);
        node_data_synced_pos = ftell(node_data);
        
        
        uint32_t page_size = sysconf(_SC_PAGESIZE);
        
        uint64_t pos = ((node.id * 2 * sizeof(uint32_t)) / page_size + 1) * page_size;
        cout << "(" << ((pos - node_index_synced_pos) / 1000000) << "MB node index)";
        cout.flush();
        
        //int res = msync( ((uint8_t*)node_index.ptr) + node_index_synced_pos, (pos - node_index_synced_pos), MS_SYNC);
        //if (res != 0) { perror("msync"); exit(0); }
        
        node_index_synced_pos = pos;
        //res = madvise( node_index.ptr,  node_index.size, MADV_DONTNEED);
        //if (res != 0) { perror("madvise node_index"); exit(0);}

        //madvise( vertex_data.ptr, vertex_data.size, MADV_DONTNEED);
        //if (res != 0) { perror("madvise vertex_data"); exit(0);}
        cout << " done";
        cout << endl << endl << "\e[2A" << "\e[s";
        
        
    }
*/
}

void OsmConsumerDumper::consumeWay ( OSMWay  &way)
{
    nWays++;
    filterTags(way.tags);
    way.serialize(*wayData, &way_index);
}

void OsmConsumerDumper::consumeRelation( OsmRelation &relation) 
{
    nRelations++;
    filterTags(relation.tags);
    relation.serializeWithIndexUpdate( *relationData, &relation_index);
    
}


