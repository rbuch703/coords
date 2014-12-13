
#define _FILE_OFFSET_BITS 64
/*
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>*/
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h> //for _SC_PAGESIZE


#include <iostream>
#include <string>
#include <map>
#include <list>
#include <set>

#include "mem_map.h"
#include "osmxmlparser.h"
#include "osm_types.h"
#include "osm_tags.h"
#include "symbolic_tags.h"

using namespace std;

const char* nodes_data_filename = "intermediate/nodes.data";
const char* nodes_index_filename= "intermediate/nodes.idx";
const char* vertices_data_filename =    "vertices.data";    //put it onto the SSD for fast random access
const char* ways_data_filename =  "intermediate/ways.data";
const char* ways_index_filename=  "intermediate/ways.idx";
const char* ways_int_data_filename="intermediate/ways_int.data"; // does not need an index, can use the same as 'ways'
const char* relations_data_filename="intermediate/relations.data";
const char* relations_index_filename="intermediate/relations.idx";

class OsmXmlDumpingParser: public OsmXmlParser
{
public:
    OsmXmlDumpingParser(FILE * f): OsmXmlParser(f), nNodes(0), nWays(0), nRelations(0)
    {
        uint32_t nSymbolicTags = sizeof(symbolic_tags_keys) / sizeof(const char*);
        assert( nSymbolicTags = sizeof(symbolic_tags_values) / sizeof(const char*)); //consistency check: #keys==#values
        assert( nSymbolicTags <= (256));  // we assign 8bit numbers to these, so there must not be more than 2^8
        for (uint32_t i = 0; i < nSymbolicTags; i++)
        {
            OSMKeyValuePair kv(symbolic_tags_keys[i], symbolic_tags_values[i]);
            symbolic_tags.insert( pair<OSMKeyValuePair, uint8_t>(kv, i));
        }
        cout << "Read " << symbolic_tags.size() << " symbolic tags" << endl;
        
        //for situations where several annotation keys exist with the same meaning
        //this dictionary maps them to a common unique key
        rename_key.insert( std::pair<std::string, std::string>("postal_code", "addr:postcode"));
        rename_key.insert( std::pair<std::string, std::string>("url", "website")); //according to OSM specs, "url" is obsolete
        rename_key.insert( std::pair<std::string, std::string>("phone", "contact:phone"));
        rename_key.insert( std::pair<std::string, std::string>("fax", "contact:fax"));
        rename_key.insert( std::pair<std::string, std::string>("email", "contact:email"));
        rename_key.insert( std::pair<std::string, std::string>("addr:housenumber", "addr:streetnumber"));
        //TODO: replace natural=lake by natural=water
        for (uint32_t i = 0; i < num_ignore_keys; i++)
            ignore_key.insert(ignore_keys[i]);
            
        truncateFile(nodes_index_filename);
        truncateFile(nodes_data_filename);
        truncateFile(vertices_data_filename);
        truncateFile(ways_index_filename);
        truncateFile(ways_data_filename);
        truncateFile(ways_int_data_filename);
        truncateFile(relations_index_filename);
        truncateFile(relations_data_filename);

    };
    virtual ~OsmXmlDumpingParser() {};
protected:

    static const string base_path;
    //pad the data file to a multiple of the page size, so that it can be opened using a memory map
    void padFile(FILE* file)
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
    }
    
    void truncateFile(string filename) {
        FILE* f= fopen(filename.c_str() , "wb+"); 
        if (f == NULL) {perror("[ERR] fopen:"); assert(false);} 
        fclose(f);
    }
    virtual void beforeParsingNodes () {
        node_index = init_mmap( nodes_index_filename );
        
        node_data = fopen(nodes_data_filename, "wb+");
        const char* node_magic = "ON10"; //OSM Nodes v. 1.0
        fwrite(node_magic, 4, 1, node_data);

        vertex_data = init_mmap(vertices_data_filename); //holds just raw vertex coordinates indexed by node_id; no tags
    };  
    
    virtual void afterParsingNodes () {
        padFile(node_data);
        fclose( node_data ); //don't need node data any more
        free_mmap(&node_index);
        
        free_mmap(&vertex_data);
        //re-open the vertex mmap read-only. This should improve Linux' mapping behavior for the next phase,
        //where ways are read and written sequentially, but vertices are read randomly
        vertex_data = init_mmap(vertices_data_filename, true, false); 
      cout << "== Done parsing Nodes ==" << endl;
    };
    
    virtual void beforeParsingWays () 
    {
        way_index = init_mmap(ways_index_filename);
        
        way_data = fopen(ways_data_filename, "wb+");
        const char* way_magic = "OW10"; //OSM Ways v. 1.0
        fwrite(way_magic, 4, 1, way_data);
        
        //way_int_index = init_mmap("ways_int.idx");

        way_int_data = fopen(ways_int_data_filename, "wb+");
        const char* way_int_magic = "OI10"; //OSM Integrated ways v. 1.0
        fwrite(way_int_magic, 4, 1, way_int_data);
        
        

    };
    
    virtual void afterParsingWays () {
        free_mmap(&vertex_data);
        free_mmap(&way_index);
        //free_mmap(&way_int_index);
        
        padFile(way_data);
        fclose(way_data);
        
        padFile(way_int_data);
        fclose(way_int_data);
        
        cout << "== Done parsing Ways ==" << endl;
    };   

    virtual void beforeParsingRelations () {
        relation_index = init_mmap(relations_index_filename);
        
        relation_data = fopen(relations_data_filename, "wb+");
        const char* relation_magic = "OR10"; //OSM Relation v. 1.0
        fwrite(relation_magic, 4, 1, relation_data);
    }; 

    virtual void afterParsingRelations () {
        free_mmap(&relation_index);
        padFile(relation_data);
        fclose( relation_data);
        
        cout << "==================== done =======================" << endl;
        cout << "statistics: " << nNodes << " nodes, " << nWays << " ways, " << nRelations << " relations" << endl;
        
    }; 

    void processTags(list<OSMKeyValuePair> &tags)
    {
        list<OSMKeyValuePair>::iterator tag = tags.begin();
        while (tag != tags.end())
        {
            if ( rename_key.count(tag->first) ) tag->first = rename_key[tag->first];
            
            if ( ignore_key.count(tag->first) ) 
            {
                tag = tags.erase(tag);
                continue;
                /*list<OSMKeyValuePair>::iterator prev = tag;
                tag++;
                tags.erase(prev);*/
            } 
            
            bool removeTag = false;
            for (unsigned int i = 0; i < num_ignore_key_prefixes; i++)
            {
                if (tag->first.find(ignore_key_prefixes[i]) == 0) //tag starts with ignore-prefix
                {
                    removeTag = true;
                    break;
                }
            }

            if (removeTag)
            {
                tag = tags.erase(tag);
                continue;
            }
            
            tag++;
        }
    }

    virtual void completedNode( OSMNode &node) 
    {
        nNodes++;
        processTags(node.tags);
        node.serialize(node_data, &node_index, &symbolic_tags);
        
        ensure_mmap_size( &vertex_data, (node.id+1) * 2 * sizeof(uint32_t));
        uint32_t* vertex_ptr = (uint32_t*)vertex_data.ptr;
        vertex_ptr[2*node.id]   = node.lat;
        vertex_ptr[2*node.id+1] = node.lon;
    }
    
    virtual void completedWay ( OSMWay  &way) 
    {
        nWays++;
        processTags(way.tags);
        // write the way itself to file
        way.serialize(way_data, &way_index, &symbolic_tags);
        
        //convert the way to an integrated way, by replacing the node indices with the actual node lat/lon
        list<OSMVertex> vertices;
        uint32_t* vertices_ptr = (uint32_t*) vertex_data.ptr;
        for (list<uint64_t>::const_iterator ref = way.refs.begin(); ref != way.refs.end(); ref++)
        {
            if (*ref == 0) //there is no node with id = 0, this is likely a dummy
                //create a dummy node so that the sizes of the way and the integrated way match
                vertices.push_back( OSMVertex( 0, 0) );
            else
                vertices.push_back( OSMVertex( vertices_ptr[*ref * 2], vertices_ptr[*ref * 2+1]) );
        }
        
        OSMIntegratedWay int_way(way.id, vertices, way.tags);
        // the "way_index" here is deliberate.
        // an integrated way always has the same size as the corresponding normal OSM way
        // (in an integrated way, the uint64_t node refs are replaced by 2xint32_t lat/lon coordinates)
        // thus, a single index is sufficient for both data files
        int_way.serialize(way_int_data, &way_index, &symbolic_tags);
    }
    
    virtual void completedRelation( OSMRelation &relation) 
    {
        nRelations++;
        processTags(relation.tags);
        relation.serialize( relation_data, &relation_index, &symbolic_tags);
        
    }
    virtual void doneParsingNodes () {cout << "===============================================" << endl;}
private:
    mmap_t node_index, vertex_data, way_index/*, way_int_index*/, relation_index;
    FILE *node_data, *way_data, *way_int_data, *relation_data;
    map<OSMKeyValuePair, uint8_t> symbolic_tags;
    map<string, string> rename_key; 
    set<string> ignore_key;    //ignore key-value pairs which are irrelevant for a viewer application
    uint64_t nNodes, nWays, nRelations;
    
};


int main()
{

    //int in_fd = open("andorra-latest.osm", O_RDONLY);
    int in_fd = 0; //default to standard input
    FILE *f = fdopen(in_fd, "rb");
    
    OsmXmlDumpingParser parser( f );
    parser.parse();
    fclose(f);
    
/*
    std::cout << "statistics\n==========" << std::endl;
    std::cout << "#nodes: " << parser.nNodes << std::endl;
    std::cout << "#ways: " << parser.nWays << std::endl;
    std::cout << "#relations: " << parser.nRelations << std::endl;
    //std::cout << "#changesets: " << state.getElementCount(CHANGESET) << std::endl;
  */  
	return 0;
	
}


