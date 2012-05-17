
#define _FILE_OFFSET_BITS 64

#include <fstream>
#include <iostream>
#include <string>
#include <list>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>

#include <map>
#include <set>
//#define EXTRACT_NODES
#undef EXTRACT_COASTLINE
#include "mem_map.h"
#include "osm_tags.h"

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


FILE * in_file = NULL;


//int32_t* map = NULL;
const char* file_name = "nodes.bin";
//int32_t fd;
//int64_t file_no_nodes = 0;

typedef enum PARENT_ELEMENT { NODE, WAY, RELATION, CHANGESET, OTHER } PARENT_ELEMENT;

int main()
{
    mmap_t node_map;
    node_map = init_mmap( file_name);
    
    //for situations where several annotation keys exist with the same meaning
    //this dictionary maps them to a common unique key
    std::map<std::string, std::string> rename_key; 
    rename_key.insert( std::pair<std::string, std::string>("postal_code", "addr:postcode"));
    rename_key.insert( std::pair<std::string, std::string>("website", "url"));
    rename_key.insert( std::pair<std::string, std::string>("phone", "contact:phone"));
    rename_key.insert( std::pair<std::string, std::string>("fax", "contact:fax"));
    rename_key.insert( std::pair<std::string, std::string>("email", "contact:email"));
    rename_key.insert( std::pair<std::string, std::string>("addr:housenumber", "addr:streetnumber"));
    
    std::set<std::string> ignore_key;    //ignore key-value pairs which are irrelevant for a viewer application

    
    for (uint32_t i = 0; i < num_ignore_keys; i++)
        ignore_key.insert(ignore_keys[i]);
    
    std::set<std::string> direct_storage_key; // keys whose values are unlikely to occur more than once and thus should be 
                                              // stored directly along with the node
    
    const char * ds_keys[]={"name", "name_en", "name:ko_rm", "name:uk", "name:ru", "name_ja", "name_de", "int_name", 
                            "name_fr", "name_ar", "name_ar1", "ref", "website", "url", "wikipedia", "note","comment","description",
                            "contact:phone", "contact:fax", "contact:email", "addr:full"};
    
    for (uint32_t i = 0; i < sizeof(ds_keys) / sizeof(const char*); i++)
        direct_storage_key.insert(ds_keys[i]);
   
    
    int in_fd = 0; //default to standard input
    //in_fd = open("isle_of_man.osm", O_RDONLY);
    in_file = fdopen(in_fd, "r"); 

    std::string line;
    int64_t nChangesets = 0;
    int64_t nNodes = 0;
    int64_t nWays = 0;
    int64_t nRelations = 0;
    std::list<int64_t> node_refs;
    
    char* line_buffer = NULL;
    size_t line_buffer_size = 0;

    std::map<std::pair<std::string, std::string>, int64_t> kv_map;
    
    PARENT_ELEMENT parent = OTHER;
    while (0 <= getline(&line_buffer, &line_buffer_size, in_file))
    {   
    
        /*line_length = ;
        if (line_length < 1)
            {perror("getline"); exit(0);}*/
        
        //getline(in, line);
        //std::string line(line_buffer);
        //std::cout << line << std::endl;
//        if (line.length() < 4) assert(false && "Unexpected empty line");
        if (strstr(line_buffer, "<changeset")) { 
            parent = CHANGESET; 
            nChangesets++;
            if (nChangesets % 1000000 == 0)
                std::cout << (nChangesets/1000000) << "M changesets processed" << std::endl;
        }
        else
        if (strstr(line_buffer, "<node"))
        //line.find("<node") != std::string::npos)
        {
            nNodes++;
            if (nNodes % 10000000 == 0)
                std::cout << (nNodes/1000000) << "M nodes processed" << std::endl;
            //continue;
            node_refs.clear();
#ifdef EXTRACT_NODES
            int32_t lat = degValueToInt(line_buffer, "lat");
            
            int32_t lon = degValueToInt(line_buffer, "lon");
            int64_t id = strtoul(getValue(line_buffer, "id"), NULL, 10);
            dumpNode(&node_map, id, lat, lon);
#endif
            parent = NODE;
        } else if (strstr(line_buffer, "<way"))
        // (line.find("<way") != std::string::npos)
        {
            nWays++;
            if (nWays % 1000000 == 0)
                std::cout << (nWays/1000000) << "M ways processed" << std::endl;
            node_refs.clear();
            parent = WAY;
        }
        else if (strstr(line_buffer, "<relation"))
        //(line.find("<relation") != std::string::npos)
        {
            nRelations++;
            if (nRelations % 1000000 == 0)
                std::cout << (nRelations/1000000) << "M relations processed" << std::endl;
            
            node_refs.clear();
            parent = RELATION;
        }
        else if (strstr(line_buffer, "<nd"))
        //(line.find("<nd") != std::string::npos)
        {
            if (parent != WAY)
            { std::cout << "[ERR] at '" << line_buffer << "'" << std::endl; assert(parent == WAY);}
            
            int64_t ref = strtoul( getValue(line_buffer, "ref"), NULL, 10);
            node_refs.push_back( ref);
            //dont set "parent", since this is a child node
        }
        else if (strstr(line_buffer, "<tag"))
        //line.find("<tag") != std::string::npos)
        {
            if (parent == OTHER)
            { std::cout << "[ERR] at '" << line_buffer << "'" << std::endl; assert (parent != OTHER);}
            std::string key = getValue(line_buffer, "k");
            if (rename_key.count(key)) key = rename_key[key];
            std::string val = getValue(line_buffer, "v");
            if (ignore_key.count(key)) continue;
            if (direct_storage_key.count(key)) continue;    
            
            std::pair<std::string, std::string> kv(key, val);
            if (kv_map.count(kv)) kv_map[kv]++;
            else kv_map.insert(std::pair<std::pair<std::string, std::string>, int64_t>(kv, 1));
            
            //if ((key == "admin_level") && (val == "2"))
            if ((key == "natural") && (val == "coastline"))
            {
#ifdef EXTRACT_COASTLINE
                print_nodes(&node_map, node_refs);
#endif
            }
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

    free_mmap(&node_map);    
    fclose(in_file);
    
    //std::list<std::pair<int64_t, std::pair<std::string, std::string> > > lst_kv;
    std::map<std::string, std::pair<int64_t, int64_t> > keys;    
    for (std::map<std::pair<std::string, std::string>, int64_t>::const_iterator it = kv_map.begin(); it!= kv_map.end(); it++)
    {
        //lst_kv.push_back( std::pair<int64_t, std::pair<std::string, std::string> >(it->second, it->first));

        std::string key = it->first.first;
        int64_t n = it->second;
        if (keys.count(key)) 
        {
            keys[key].first+=1;
            keys[key].second+=n;
        }
        else keys.insert( std::pair<std::string, std::pair<int64_t, int64_t> >(key, std::pair<int64_t, int64_t>(1, n)));
    }
    
    for (std::map<std::string, std::pair<int64_t, int64_t> >::const_iterator it = keys.begin(); it != keys.end(); it++)
    {
        std::cerr << it->first << "§" << it->second.first << "§" << it->second.second << std::endl;
    }
    /*
    lst_kv.sort();
    lst_kv.reverse();



    for (std::list<std::pair<int64_t, std::pair<std::string, std::string> > >::const_iterator it = lst_kv.begin(); it!= lst_kv.end(); it++)
    {
        //std::cerr << it->first << "§" << it->second.first << "§" << it->second.second << std::endl;
    }*/
        
    std::cout << "statistics\n==========" << std::endl;
    std::cout << "#nodes: " << nNodes << std::endl;
    std::cout << "#ways: " << nWays << std::endl;
    std::cout << "#relations: " << nRelations << std::endl;
    std::cout << "#changesets: " << nChangesets << std::endl;
    std::cout << "#key-value pairs: " << kv_map.size() << std::endl;

	return 0;
	
}


