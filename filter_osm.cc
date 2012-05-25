
#define _FILE_OFFSET_BITS 64

#include <fstream>
#include <iostream>
#include <string>
#include <list>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>

#include <map>
#include <set>
#include "mem_map.h"

#include "osmxmlparser.h"

#include "osm_types.h"
#include "osm_tags.h"

#include "symbolic_tags.h"

using namespace std;


class OsmXmlDumpingParser: public OsmXmlParser
{
public:
    OsmXmlDumpingParser(FILE * f): OsmXmlParser(f) 
    {
        uint32_t nSymbolicTags = sizeof(symbolic_tags_keys) / sizeof(const char*);
        assert( nSymbolicTags = sizeof(symbolic_tags_values) / sizeof(const char*)); //consistence check #keys<->#values
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
    
        for (uint32_t i = 0; i < num_ignore_keys; i++)
            ignore_key.insert(ignore_keys[i]);
            
        truncateFile("nodes.idx");
        truncateFile("nodes.data");
        truncateFile("/mnt/ssd/vertices.data");
        truncateFile("ways.idx");
        truncateFile("ways_int.idx");
        truncateFile("relations.idx");
        truncateFile("ways.data");
        truncateFile("ways_int.data");
        truncateFile("relations.data");

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
/*        FILE* f= fopen(filename.c_str() , "wb+"); 
        if (f == NULL) {perror("[ERR] fopen:"); exit(0);} 
        fclose(f);*/
    }
    virtual void beforeParsingNodes () {
        node_index = init_mmap( "nodes.idx" );
        
        node_data = fopen("nodes.data", "wb+");
        const char* node_magic = "ON10"; //OSM Nodes v. 1.0
        fwrite(node_magic, 4, 1, node_data);

        vertex_data = init_mmap("/mnt/ssd/vertices.data"); //holds just raw vertex coordinates indexed by node_id; no tags
    };  
    
    virtual void afterParsingNodes () {
        padFile(node_data);
        fclose( node_data ); //don't need node data any more
        free_mmap(&node_index);
        /** do not free the vertex map. We still it, because generating
          * integrated ways (ways that include vertices) depends on it.*/
      cout << "== Done parsing Nodes ==" << endl;
    };
    
    virtual void beforeParsingWays () 
    {
        way_index = init_mmap("ways.idx");
        
        way_data = fopen("ways.data", "wb+");
        const char* way_magic = "OW10"; //OSM Ways v. 1.0
        fwrite(way_magic, 4, 1, way_data);
        
        //way_int_index = init_mmap("ways_int.idx");

        way_int_data = fopen("ways_int.data", "wb+");
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
        relation_index = init_mmap("relations.idx");
        
        relation_data = fopen("relations.data", "wb+");
        const char* relation_magic = "OR10"; //OSM Relation v. 1.0
        fwrite(relation_magic, 4, 1, relation_data);
    }; 

    virtual void afterParsingRelations () {
        free_mmap(&relation_index);
        padFile(relation_data);
        fclose( relation_data);
    }; 

    void processTags(list<OSMKeyValuePair> &tags)
    {
        list<OSMKeyValuePair>::iterator tag = tags.begin();
        while (tag != tags.end())
        {
            if ( rename_key.count(tag->first) ) tag->first = rename_key[tag->first];
            
            if ( ignore_key.count(tag->first) ) 
            {
                list<OSMKeyValuePair>::iterator prev = tag;
                tag++;
                tags.erase(prev);
            } else tag++;
        }
    }

    virtual void completedNode( OSMNode &node) 
    {
/*        processTags(node.tags);
        node.serialize(node_data, &node_index, symbolic_tags);
        
        ensure_mmap_size( &vertex_data, (node.id+1) * 2 * sizeof(uint32_t));
        uint32_t* vertex_ptr = (uint32_t*)vertex_data.ptr;
        vertex_ptr[2*node.id]   = node.lat;
        vertex_ptr[2*node.id+1] = node.lon;
*/    }
    
    virtual void completedWay ( OSMWay  &way) 
    {
/*        processTags(way.tags);
        way.serialize(way_data, &way_index, symbolic_tags);
        
        list<Vertex> vertices;
        uint32_t* vertices_ptr = (uint32_t*) vertex_data.ptr;
        for (list<uint64_t>::const_iterator ref = way.refs.begin(); ref != way.refs.end(); ref++)
        {
            if (*ref == 0) //there is no node with id = 0, this is likely a dummy
                //create a dummy node so that the sizes of the way and the integrated way match
                vertices.push_back( Vertex( 0, 0) );
            else
                vertices.push_back( Vertex( vertices_ptr[*ref * 2], vertices_ptr[*ref * 2+1]) );
        }
        
        OSMIntegratedWay int_way(way.id, vertices, way.tags);
        // the "way_index" here is deliberate.
        // an integrated way always has the same size as the corresponding normal OSM way
        // (in an integrated way, the uint64_t node refs are replaced by 2xint32_t lat/lon coordinates)
        // thus, a single index is sufficient for both data files
        int_way.serialize(way_int_data, &way_index, symbolic_tags);
*/    }
    
    virtual void completedRelation( OSMRelation &relation) 
    {
        processTags(relation.tags);
        relation.serialize( relation_data, &relation_index, symbolic_tags);
    }
    virtual void doneParsingNodes () {cout << "===============================================" << endl;}
private:
    mmap_t node_index, vertex_data, way_index/*, way_int_index*/, relation_index;
    FILE *node_data, *way_data, *way_int_data, *relation_data;
    map<OSMKeyValuePair, uint8_t> symbolic_tags;
    map<string, string> rename_key; 
    set<string> ignore_key;    //ignore key-value pairs which are irrelevant for a viewer application
};


class OsmXmlCountingParser: public OsmXmlParser
{
public:
    OsmXmlCountingParser(FILE* f): OsmXmlParser(f), nWays(0), nRelations(0), nNodes(0) {}
    virtual void completedNode    ( OSMNode &) {nNodes++;}
    virtual void completedWay     ( OSMWay  &) {nWays++;}
    virtual void completedRelation( OSMRelation &) {nRelations++;}

    uint64_t nWays, nRelations, nNodes;
};

int main()
{

    //in_fd = open("isle_of_man.osm", O_RDONLY);
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


