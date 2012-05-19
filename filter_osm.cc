
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
#include "osm_tags.h"

//#include "symbolic_tags.h"

#define CREATE_KV_HISTOGRAM
using namespace std;

typedef pair<string, string> OSMKeyValuePair;

mmap_t node_index;
FILE* node_data;
map<OSMKeyValuePair, uint16_t> symbolic_tags;

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


/*
std::string getValue(const std::string &line, const std::string &key)
{
    int in_pos = line.find(key+"=\"") + key.length()+2;
    assert( in_pos != std::string::npos);
    int out_pos = line.find("\"", in_pos);
    assert( out_pos != std::string::npos);
    
    return line.substr(in_pos, out_pos - in_pos);
}*/

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
int degToInt(std::string deg)
{
    bool is_negative = (deg[0] == '-');
    int start_pos = is_negative ? 1:0;
    int decimal_pos = deg.find(".");
    assert( decimal_pos != std::string::npos);
    std::string s = deg.substr(start_pos, decimal_pos - start_pos);
    int32_t integer = atoi(s.c_str());
    assert( abs(integer) <= 180 );
    s = deg.substr(decimal_pos+1);
    assert( s.length() <= 7);
    while (s.length() < 7) s+='0';
    int32_t frac = atoi(s.c_str());
    int32_t res = integer*10000000 + frac;
    if (is_negative) res = -res;
    //std::cout << deg << "=>" << res << std::endl;
    return res;
    
}*/


void dumpNode(mmap_t *node_map, int64_t node_id, int32_t lat, int32_t lon)
{
    //temporary nodes in OSM editors are allowed to have negative node IDs, but those in the official maps should be guaranteed to be positive
    //also, negative ids would mess up the memory map
    assert (node_id > 0);  

    ensure_mmap_size(node_map, (node_id+1)* 2*sizeof(int32_t));
    int32_t* map = (int32_t*)node_map->ptr;
    map[node_id*2] = lat;
    map[node_id*2+1] = lon;
}


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


//int32_t* map = NULL;
//const char* file_name = "nodes.bin";
//int32_t fd;
//int64_t file_no_nodes = 0;

typedef enum ELEMENT { NODE, WAY, RELATION, CHANGESET, OTHER } ELEMENT;
const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};


struct OSMRelationMember
{
    OSMRelationMember( ELEMENT member_type, uint64_t member_ref, string member_role):
        type(member_type), ref(member_ref), role(member_role) { }
    ELEMENT type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    string role;   //the role the member play to the relation
};

struct OSMNode
{
    uint32_t lat;
    uint32_t lon;
    uint64_t id;
    list<OSMKeyValuePair> tags;
};

struct OSMWay
{
    uint64_t id;
    list<OSMKeyValuePair> tags;
};

struct OSMRelation
{
    uint64_t id;
    list<OSMRelationMember> members;
    list<OSMKeyValuePair> tags;
};

/** on-file layout:
    uint16_t num_symbolic_tags;
    uint16_t num_verbose_tags;
    uint16_t symbolic_tags[num_symbolic_tags];
    uint64_t verbose_tag_data_len; (total data size of all verbose tags, in bytes); optional, present only if num_verbose_tags > 0
    <2*num_verbose_tags> zero-terminated strings containing interleaved keys and values 
    (key_0, value_0, key_1, value_1, ...)

*/
void serializeTags( const list<OSMKeyValuePair> tags, FILE* file, const map<OSMKeyValuePair, uint16_t> &tag_symbols)
{
    list<OSMKeyValuePair> verbose_tags;
    list<uint16_t> symbolic_tags;
    
    uint64_t verbose_tag_data_len = 0;
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
    {
        if ( tag_symbols.count(*it)) symbolic_tags.push_back( tag_symbols.find(*it)->second);
        else 
        {
            verbose_tag_data_len += strlen(it->first.c_str() )+1; //including null-termination
            verbose_tag_data_len += strlen(it->second.c_str())+1; //including null-termination
            verbose_tags.push_back(*it);
        }
    }
    
    assert(symbolic_tags.size() < (1<<16));
    uint16_t num_symbolic_tags = symbolic_tags.size();
    fwrite(&num_symbolic_tags, sizeof(num_symbolic_tags), 1, file);
    
    assert(verbose_tags.size() < (1<<16));
    uint16_t num_verbose_tags = verbose_tags.size();
    fwrite(&num_verbose_tags, sizeof(num_verbose_tags), 1, file);
    
    for (list<uint16_t>::const_iterator it = symbolic_tags.begin(); it!= symbolic_tags.end(); it++)
    {
        uint16_t tmp = *it;
        fwrite( &tmp, sizeof(tmp), 1, file);
    }
    
    if (num_verbose_tags == 0) return;
    fwrite( &verbose_tag_data_len, sizeof(verbose_tag_data_len), 1, file);
    for (list<OSMKeyValuePair>::const_iterator it = verbose_tags.begin(); it!= verbose_tags.end(); it++)
    {
        fwrite( it->first.c_str(),  strlen(it->first.c_str()) + 1, 1, file);    //both including their null-termination
        fwrite( it->second.c_str(), strlen(it->second.c_str())+ 1, 1, file);
    }
}

void serializeNode( const OSMNode &node, FILE* data_file, mmap_t *index_map)
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    assert (node.id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped node *starts*
    fwrite( &node.lat, sizeof(node.lat), 1, data_file);
    fwrite( &node.lon, sizeof(node.lon), 1, data_file);

    //map<OSMKeyValuePair, uint16_t> dummy; //empty dummy map TODO: replace by actual map of most-frequently used kv-pairs
    serializeTags( node.tags, data_file, symbolic_tags );
    
    ensure_mmap_size( index_map, (node.id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[node.id] = offset;
}

class ParserState
{
public:
    ParserState(): current_parent(OTHER)
    {
        for (int i = 0; i < nElements; i++) element_count[i] = 0;
    }    
   
    void update( ELEMENT new_element)
    {
        switch (new_element)
        {
            case NODE:
            case WAY:
            case RELATION:
            case CHANGESET:
            
                if (current_parent == NODE)
                {
                    OSMNode n;
                    n.lat = node_lat;
                    n.lon = node_lon;
                    n.id =  node_id;
                    n.tags = tags;
                    serializeNode(n, node_data, &node_index);
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
    
    list<int64_t> node_refs;
    list<pair<string, string> > tags;
    list<OSMRelationMember> members;
    
    int32_t node_lat, node_lon;
    uint64_t node_id, way_id, relation_id;
};


int main()
{
    //mmap_t node_index;
    /*uint32_t nSymbolicTags = sizeof(symbolic_tags_keys) / sizeof(const char*);
    assert( nSymbolicTags = sizeof(symbolic_tags_values) / sizeof(const char*)); //consistence check #keys<->#values
    assert( nSymbolicTags <= (1<<16));  // we assign 16bit numbers to these, so there must not be more than 2^16
    for (uint32_t i = 0; i < nSymbolicTags; i++)
    {
        OSMKeyValuePair kv(symbolic_tags_keys[i], symbolic_tags_values[i]);
        symbolic_tags.insert( pair<OSMKeyValuePair, uint16_t>(kv, i));
    }
    cout << "Read " << symbolic_tags.size() << " symbolic tags" << endl;
    */
    
    truncate("node.idx", 0);    // clear indices. Since the data itself is overwritten, old indices are invalid
    node_index = init_mmap( "node.idx");
    
    node_data = fopen("node.db", "wb");
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
   
    
    int in_fd = 0; //default to standard input
    //in_fd = open("isle_of_man.osm", O_RDONLY);
    FILE * in_file = fdopen(in_fd, "r"); 

    //std::string line;
    
    char* line_buffer = NULL;
    size_t line_buffer_size = 0;

#ifdef CREATE_KV_HISTOGRAM
    #warning creating key-value histogram. This is going to be slooooow
    map<OSMKeyValuePair, int64_t> kv_map;
#endif
    
//    PARENT_ELEMENT parent = OTHER;
    
    ParserState state;
    
    while (0 <= getline(&line_buffer, &line_buffer_size, in_file))
    {   

        if (strstr(line_buffer, "<changeset")) { state.update(CHANGESET); }
        else if (strstr(line_buffer, "<node"))
        {
            state.update(NODE);
            //continue;
            int32_t lat = degValueToInt(line_buffer, "lat"); 
            int32_t lon = degValueToInt(line_buffer, "lon");
            int64_t id = strtoul(getValue(line_buffer, "id"), NULL, 10);
            state.setNodeProperties(lat, lon, id);

            //dumpNode(&node_map, id, lat, lon);
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
        //line.find("<tag") != std::string::npos)
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
            if (direct_storage_key.count(key)) continue;    
            
            state.addTag(key, val);

#ifdef CREATE_KV_HISTOGRAM
            pair<string, string> kv(key, val);
            if (kv_map.count(kv)) kv_map[kv]++;
            else kv_map.insert(std::pair<std::pair<std::string, std::string>, int64_t>(kv, 1));
            if (kv_map.size() > 15000000) break; //FIXME: arbitrary break to prevent OOM
#endif
            //if ((key == "admin_level") && (val == "2"))
#undef EXTRACT_COASTLINE
#ifdef EXTRACT_COASTLINE
            if ((key == "natural") && (val == "coastline"))
            {
                print_nodes(&node_map, node_refs);
            }
#endif
            //std::cerr << key << "\t" << val << std::endl;
        } 
        else if (strstr(line_buffer, "<member ")) 
        { 
            //TODO: parse relation members
            //TODO: make sure parent element is "relation"
        }
        
        else if (strstr(line_buffer, "<?xml ")) { continue;}
        else if (strstr(line_buffer, "<osm ")) { continue;}
        else if (strstr(line_buffer, "</")) { continue;}
        else if (strstr(line_buffer, "<!--")) { continue;}
        else if (strstr(line_buffer, "<bound box")) { continue;}
        else
        {
            printf("[ERROR] unknown tag in %s\n", line_buffer);
            //exit(0);
        }
    } //while (line_buffer[line_length-1] == '\n');
    free (line_buffer);

    free_mmap(&node_index);    
    fclose(node_data);
    
    fclose(in_file);
    

#ifdef CREATE_KV_HISTOGRAM
    std::list<std::pair<int64_t, std::pair<std::string, std::string> > > lst_kv;
    
    for (std::map<std::pair<std::string, std::string>, int64_t>::const_iterator it = kv_map.begin(); it!= kv_map.end(); it++)
    {
        if (it->second > 10000)
            lst_kv.push_back( std::pair<int64_t, std::pair<std::string, std::string> >(it->second, it->first));
    }

#define DUMP_KV_FREQUENCY
#ifdef DUMP_KV_FREQUENCY
    ofstream of_kvs;
    of_kvs.open("kv_.csv");

    lst_kv.sort();
    lst_kv.reverse();

    for (std::list<std::pair<int64_t, std::pair<std::string, std::string> > >::const_iterator it = lst_kv.begin(); it!= lst_kv.end(); it++)
    {
        of_kvs << it->first << "§" << it->second.first << "§" << it->second.second << std::endl;
    }
    of_kvs.close();
#endif
    
#define DUMP_KEY_FREQUENCY
#ifdef  DUMP_KEY_FREQUENCY
    ofstream of_keys;
    of_keys.open("keys_.csv");
    
    std::map<std::string, int64_t> keys;    

    for (map<pair<string, string>, int64_t>::const_iterator it = kv_map.begin(); it!= kv_map.end(); it++)
    {
        std::string key = it->first.first;
        int64_t n = it->second;
        if (keys.count(key))  { keys[key] +=n; }
        else keys.insert( std::pair<std::string, int64_t>(key, n));
    }
    
    for (map<string, int64_t>::const_iterator it = keys.begin(); it != keys.end(); it++)
        of_keys << it->first << "§" << it->second << std::endl;
    of_keys.close();
#endif

#endif

#if 0
    FILE* f_kv_data = fopen("kv.db", "wb");
    
    const char* magic_kv_data  = "KVD1";    //key-value data, version 1
    //besides their usual purposes, these magic bytes also ensure that no key-value pair ever has the file offset "0",
    //and thus zero can be used to return an error in related functions

    //fwrite(magic_kv_index, 4, 1, f_kv_index);
    fwrite(magic_kv_data, 4, 1, f_kv_data);
    for (std::map<std::pair<std::string, std::string>, int64_t>::iterator it = kv_map.begin(); it!= kv_map.end(); it++)
    {
        const char* key = it->first.first.c_str();
        const char* val = it->first.second.c_str();
        //std::cerr << key << "§" << val << "§" << it->second << std::endl;
    
        uint32_t kv_data_offset = ftell(f_kv_data);    
        int32_t key_len = strlen(key);  //TODO: determine how much memory is saved when changing this to int16_t
        fwrite(&key_len, sizeof(key_len), 1, f_kv_data);
        fwrite(key, strlen(key), 1, f_kv_data);
        
        int32_t val_len = strlen(val);
        fwrite(&val_len, sizeof(val_len), 1, f_kv_data);
        fwrite(val, strlen(val), 1, f_kv_data);
        
        //fwrite(&kv_data_offset, sizeof(kv_data_offset), 1, f_kv_index);
        
        /** so far the dictionary mapped kv-pairs to their number of occurrence
          * we don't care about that number any more, so we abuse to dictionary
          * to now map each key-value pair to its file position 
          * (with which KV-pairs from nodes, ways and relations will later be replaced with on file);
          */ 
        it->second = kv_data_offset;    
    }    
    fclose(f_kv_data);
#endif
        
    std::cout << "statistics\n==========" << std::endl;
    std::cout << "#nodes: " << state.getElementCount(NODE) << std::endl;
    std::cout << "#ways: " << state.getElementCount(WAY) << std::endl;
    std::cout << "#relations: " << state.getElementCount(RELATION) << std::endl;
    std::cout << "#changesets: " << state.getElementCount(CHANGESET) << std::endl;
#ifdef CREATE_KV_HISTOGRAM
    std::cout << "#key-value pairs: " << kv_map.size() << std::endl;
#endif
	return 0;
	
}


