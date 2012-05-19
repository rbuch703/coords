
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

#include "osm_types.h"
#include "osm_tags.h"

#include "symbolic_tags.h"

using namespace std;

mmap_t node_index, way_index, relation_index;
FILE* data_file;
map<OSMKeyValuePair, uint8_t> symbolic_tags;

const char* getValue(const char* line, const char* key)
{
    static char* buffer = NULL;
    static uint32_t buffer_size = 0;

    uint32_t len = strlen(key)+3;
    if (buffer_size < len)
    {
        if (buffer) free(buffer);
        buffer = (char*)malloc(len);
        buffer_size = strlen(key)+3;
    }
    strcpy(buffer, key);
    buffer[ len-3] = '=';
    buffer[ len-2] = '"';
    buffer[ len-1] = '\0';
    
    const char* in = strstr(line, buffer);
    assert (in != NULL);
    in += (strlen(buffer)); //skip key + '="'
    const char* out = strstr(in, "\"");
    assert(out != NULL);

    uint32_t size = out - in;
    if (buffer_size <= size)
    {
        if (buffer) free(buffer);
        buffer = (char*)malloc(size+1);
        buffer_size = size+1;
    }
    memcpy(buffer, in, size);
    buffer[size] ='\0';
    //std::cout << buffer << std::endl;
    return buffer;
}



/** find the value associated to 'key' in 'line', and return its degree value as an int32_t 
    (by multiplying the float value by 10,000,000*/
int32_t degValueToInt(const char* line, const char* key)
{
    const char* in = strstr(line, key);
    assert (in != NULL);
    in += (strlen(key) + 2); //skip key + '="'
    bool isNegative = false;
    if (*in == '-')
    { isNegative = true; in++;}
    
    int32_t val = 0;
    bool after_decimal_point = false;
    int n_digits = 0;
    for (; *in != '"'; in++)
    {
        if (*in == '.') {after_decimal_point = true; continue;}
        assert (*in >= '0' && *in <= '9' && "Not a digit");
        val = val * 10 + (*in - 0x30);
        if (after_decimal_point) n_digits++;
    }
    for (; n_digits < 7; n_digits++) val*=10;
    return isNegative? -val : val;
}


/*
void dumpNode(mmap_t *node_map, int64_t node_id, int32_t lat, int32_t lon)
{
    //temporary nodes in OSM editors are allowed to have negative node IDs, but those in the official maps should be guaranteed to be positive
    //also, negative ids would mess up the memory map
    assert (node_id > 0);  

    ensure_mmap_size(node_map, (node_id+1)* 2*sizeof(int32_t));
    int32_t* map = (int32_t*)node_map->ptr;
    map[node_id*2] = lat;
    map[node_id*2+1] = lon;
} */

#if 0
void print_nodes( mmap_t *node_map, std::list<int64_t> node_refs)
{
    static int seg_count = 0;
    int c = seg_count;
    std::string seg_str;
    while (c > 0) 
    {
        seg_str = (char)(c % 10 +0x30) + seg_str;
        c = c/10;
    }
    while (seg_str.length() < 6) seg_str = '0' + seg_str;
    
    std::ofstream out;
    out.open((std::string("coastline_segments/seg_")+ seg_str + ".seg").c_str());
    uint32_t* map = (uint32_t*)node_map->ptr;
    for (std::list<int64_t>::const_iterator node = node_refs.begin(); node != node_refs.end(); node++)
    {
        assert(*node < (node_map->size / 8) && "Reading beyond end of map");
        assert(*node >= 0 && "Negative node number");
        if ((map[*node * 2] != 0) && (map[*node * 2 + 1] != 0))
            out << map[*node * 2+1] << "," << map[*node * 2] <<"," << (*node) << std::endl;
    }
//            std::cerr << map[*node *2+1] << ", ";
/*    std::cerr << std::endl;
    for (std::list<int64_t>::const_iterator node = node_refs.begin(); node != node_refs.end(); node++)
        if ((map[*node * 2] != 0) && (map[*node * 2 + 1] != 0))
            std::cerr << map[*node *2] << ", ";
    std::cerr << std::endl;*/
    out.close();
    seg_count++;
}
#endif

//int32_t* map = NULL;
//const char* file_name = "nodes.bin";
//int32_t fd;
//int64_t file_no_nodes = 0;

static const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};

class ParserState
{
public:
    ParserState(): current_parent(OTHER)
    {
        for (int i = 0; i < nElements; i++) element_count[i] = 0;
    }    

   
    //FIXME: current approach (serialize on element once the next one is discovered) does not dump the last relation
    void update( ELEMENT new_element)
    {
        switch (new_element)
        {
            case NODE:
            case WAY:
            case RELATION:
            case CHANGESET:
                switch (current_parent)
                {
                    case NODE:
                        OSMNode (node_lat, node_lon, node_id, tags).serialize( data_file, &node_index, symbolic_tags);
                        break;
                    case WAY:
                        OSMWay( way_id, node_refs, tags).serialize( data_file, &way_index, symbolic_tags);
                        break;
                    case RELATION:
                        OSMRelation(relation_id, members, tags).serialize(data_file, &relation_index, symbolic_tags);
                        break;
                    default:
                        break;
                }            
                current_parent = new_element;
                node_refs.clear();
                tags.clear();
                members.clear();
                element_count[new_element]++;
                if (element_count[new_element] % 1000000 == 0)
                    std::cout <<  (element_count[new_element]/1000000) << "M " 
                              << ELEMENT_NAMES[new_element] << "s processed" << std::endl;
                break;
            case OTHER:
                break;
        }
    }
    
    void setNodeProperties( int32_t lat, int32_t lon, uint64_t id) { node_lat = lat; node_lon = lon; node_id = id;}
    void setWayProperties(uint64_t id){way_id = id;}
    void setRelationProperties(uint64_t id) { relation_id = id;}
    
    void addNodeRef(uint64_t ref) { node_refs.push_back(ref); }
    void addTag (string key, string value) { tags.push_back( pair<string,string>(key, value));}
    void addMember(ELEMENT type, uint64_t ref, string role) {
        assert(current_parent == RELATION);
        assert( type == NODE || type == WAY || type == RELATION);
        members.push_back( OSMRelationMember(type, ref, role));
    }
    
    ELEMENT getCurrentElement() const { return current_parent;}
    uint64_t getElementCount( ELEMENT el) const { return element_count[el];}

private:
    static const int nElements = 5;

private:
    ELEMENT current_parent;
    uint64_t element_count[nElements];
    
    list<uint64_t> node_refs;
    list<pair<string, string> > tags;
    list<OSMRelationMember> members;
    
    int32_t node_lat, node_lon;
    uint64_t node_id, way_id, relation_id;
};


int main()
{
    //#warning symbolic tags disabled. This may dramatically increase the node_db size
    uint32_t nSymbolicTags = sizeof(symbolic_tags_keys) / sizeof(const char*);
    assert( nSymbolicTags = sizeof(symbolic_tags_values) / sizeof(const char*)); //consistence check #keys<->#values
    assert( nSymbolicTags <= (256));  // we assign 8bit numbers to these, so there must not be more than 2^8
    for (uint32_t i = 0; i < nSymbolicTags; i++)
    {
        OSMKeyValuePair kv(symbolic_tags_keys[i], symbolic_tags_values[i]);
        symbolic_tags.insert( pair<OSMKeyValuePair, uint8_t>(kv, i));
    }
    cout << "Read " << symbolic_tags.size() << " symbolic tags" << endl;

    
    // clear indices. Since the data itself is overwritten, old indices are invalid
    FILE *f;
    f= fopen("node.idx"    , "wb"); if (f == NULL) {perror("[ERR] fopen:"); exit(0);} fclose(f);
    f= fopen("way.idx"     , "wb"); if (f == NULL) {perror("[ERR] fopen:"); exit(0);} fclose(f);
    f= fopen("relation.idx", "wb"); if (f == NULL) {perror("[ERR] fopen:"); exit(0);} fclose(f);
    
    node_index     = init_mmap( "node.idx" );
    way_index      = init_mmap( "way.idx" );
    relation_index = init_mmap( "relation.idx" );
    
    data_file = fopen("osm.db", "wb");
    //for situations where several annotation keys exist with the same meaning
    //this dictionary maps them to a common unique key
    std::map<std::string, std::string> rename_key; 
    rename_key.insert( std::pair<std::string, std::string>("postal_code", "addr:postcode"));
    rename_key.insert( std::pair<std::string, std::string>("url", "website")); //according to OSM specs, "url" is obsolete
    rename_key.insert( std::pair<std::string, std::string>("phone", "contact:phone"));
    rename_key.insert( std::pair<std::string, std::string>("fax", "contact:fax"));
    rename_key.insert( std::pair<std::string, std::string>("email", "contact:email"));
    rename_key.insert( std::pair<std::string, std::string>("addr:housenumber", "addr:streetnumber"));
    
    std::set<std::string> ignore_key;    //ignore key-value pairs which are irrelevant for a viewer application
    for (uint32_t i = 0; i < num_ignore_keys; i++)
        ignore_key.insert(ignore_keys[i]);

    /*
    std::set<std::string> direct_storage_key; // keys whose values are unlikely to occur more than once and thus should be 
                                              // stored directly along with the node
    //TODO: find a way to verify this list. The assumption that the values for these keys are too various for them
    //      to be some of the most-often used KV-combinations is only an educated guess
    const char * ds_keys[]={"name", "name:en", "name:ko_rm", "name:uk", "name:ru", "name:ja", "name:de", "name_1", "name_2", "int_name", 
                            "name:fr", "name:ar", "name:ar1", "ele", "ref", "website", "url", "wikipedia", "note",
                            "comment","description",
                            "contact:phone", "contact:fax", "contact:email", "addr:full", "wikipedia:de", "wikipedia:ru", "alt_name"};
    
    for (uint32_t i = 0; i < sizeof(ds_keys) / sizeof(const char*); i++)
        direct_storage_key.insert(ds_keys[i]);
    */
    
    int in_fd = 0; //default to standard input
    //in_fd = open("isle_of_man.osm", O_RDONLY);
    FILE * in_file = fdopen(in_fd, "r"); 

    char* line_buffer = NULL;
    size_t line_buffer_size = 0;
    
    ParserState state;
    
    while (0 <= getline(&line_buffer, &line_buffer_size, in_file))
    {   
        const char* line = line_buffer;
        while (*line == ' ') line++;
        
        //TODO: replace strstr(line_buffer, ...) by strcmp(line, ...)
        
        if (strstr(line_buffer, "<changeset")) { state.update(CHANGESET); }
        else if (strstr(line_buffer, "<node"))
        {
            state.update(NODE);
            //continue;
            int32_t lat = degValueToInt(line_buffer, "lat"); 
            int32_t lon = degValueToInt(line_buffer, "lon");
            int64_t id = strtoul(getValue(line_buffer, "id"), NULL, 10);
            state.setNodeProperties(lat, lon, id);

        } else if (strstr(line_buffer, "<way")) { 
            state.update(WAY); 
            state.setWayProperties( strtoul(getValue(line_buffer, "id"), NULL, 10));
        }
        else if (strstr(line_buffer, "<relation")) 
        { 
            state.update(RELATION); 
            state.setRelationProperties( strtoul(getValue(line_buffer, "id"), NULL, 10));
        }
        else if (strstr(line_buffer, "<nd"))
        {
            if (state.getCurrentElement() != WAY)
            { std::cout << "[ERR] <nd> element whose parent is not a WAY at " << std::endl;
              std::cout << "\t'" << line_buffer << "'" << std::endl; }
            //do not set "parent", since this is a child node inside a WAY node
            //for the same reason do not clear 'node_refs' or 'tags'
            
            state.addNodeRef( strtoul( getValue(line_buffer, "ref"), NULL, 10) );
        }
        else if (strstr(line_buffer, "<tag"))
        {
            if (state.getCurrentElement() == OTHER)
            { std::cout << "[ERR] <tag> element whose parent is not a NODE, WAY, RELATION or CHANGESET, at " << std::endl;
              std::cout << "\t'" << line_buffer << "'" << std::endl; }
            //do not set "parent", since this is a child node inside a WAY node
            //for the same reason do not clear 'node_refs' or 'tags'
              
            std::string key = getValue(line_buffer, "k");
            if (rename_key.count(key)) key = rename_key[key];
            std::string val = getValue(line_buffer, "v");
            if (ignore_key.count(key)) continue;
            
            state.addTag(key, val);

#undef EXTRACT_COASTLINE
#ifdef EXTRACT_COASTLINE
            if ((key == "natural") && (val == "coastline"))
            {
                print_nodes(&node_map, node_refs);
            }
#endif
        } 
        else if (strstr(line_buffer, "<member ")) 
        { 
            if (state.getCurrentElement() != RELATION)
            {printf("[ERR] relation member outside of relation element at: \n\t'%s'\n", line_buffer); continue;}
            
            const char* type_str = getValue(line_buffer, "type");
            ELEMENT type;
            if (strcmp(type_str, "node") == 0) type = NODE;
            else if (strcmp(type_str, "way") == 0) type = WAY;
            else if (strcmp(type_str, "relation") == 0) type = RELATION;
            else {printf("[ERR] relation member with unknown type '%s'\n", type_str); continue;}
            
            uint64_t ref = strtoul( getValue(line_buffer, "ref"), NULL, 10);
            
            string role = getValue(line_buffer, "role");
            state.addMember(type, ref, role);
        }
        
        else if (strstr(line_buffer, "<?xml ")) { continue;}
        else if (strstr(line_buffer, "<osm ")) { continue;}
        else if (strstr(line_buffer, "</")) { continue;}
        else if (strstr(line_buffer, "<!--")) { continue;}
        else if (strstr(line_buffer, "<bound box")) { continue;}
        else
        {
            printf("[ERROR] unknown tag in %s\n", line_buffer);
        }
    }
    free (line_buffer);

    free_mmap(&node_index);    
    fclose(data_file);
    
    fclose(in_file);
    
    std::cout << "statistics\n==========" << std::endl;
    std::cout << "#nodes: " << state.getElementCount(NODE) << std::endl;
    std::cout << "#ways: " << state.getElementCount(WAY) << std::endl;
    std::cout << "#relations: " << state.getElementCount(RELATION) << std::endl;
    std::cout << "#changesets: " << state.getElementCount(CHANGESET) << std::endl;
	return 0;
	
}


