
#define _FILE_OFFSET_BITS 64

#include <fstream>
#include <iostream>
#include <string>
#include <list>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>

#define EXTRACT_NODES

const char* getValue(const char* line, const char* key)
{
    const char* in = strstr(line, key);
    assert (in != NULL);
    in += (strlen(key) + 2); //skip key + '="'
    const char* out = strstr(in, "\"");
    assert(out != NULL);

    static char* buffer = NULL;
    static int buffer_size = 0;
    int size = out - in;
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


int32_t* map = NULL;
const char* filename = "nodes.bin";
int32_t fd;
int64_t file_no_nodes = 0;
void dumpNode(int64_t node_id, int32_t lat, int32_t lon)
{
    assert (node_id > 0);    //temporary nodes in OSM editors are allowed to have negative node IDs, but those in the official maps should be guaranteed to be positive
    
    if (file_no_nodes <= node_id)
    {
        //std::cout << "File can hold at most " << file_no_nodes << " nodes, but " << node_id << " nodes need to be stored" << std::endl;

        if (map) munmap(map, file_no_nodes * 2*sizeof(int32_t));

        size_t ps = sysconf (_SC_PAGESIZE);
        
        size_t new_file_size = ((node_id+1)*2*sizeof(int32_t)+ps-1) / ps * ps;
        assert(new_file_size % ps == 0); //needs to be multiple of page size for mmap
        if (0 != ftruncate(fd, new_file_size))
            { perror("[ERR]"); exit(0);}
        file_no_nodes = new_file_size / (2*sizeof(int32_t));
        
        //std::cout << "Resizing file to hold " << file_no_nodes << " nodes (" << (file_no_nodes / ps) << " pages)" << std::endl;
        
        //WARNING: file is mapped write-only!!!
        map = (int32_t*)mmap(NULL, new_file_size, PROT_WRITE, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED)
        { perror("[ERR]"); exit(0);}
    }
    map[node_id*2] = lat;
    map[node_id*2+1] = lon;
    
}


#if 1
void print_nodes( std::list<int64_t> node_refs)
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

    for (std::list<int64_t>::const_iterator node = node_refs.begin(); node != node_refs.end(); node++)
    {
        assert(*node < file_no_nodes && "Reading beyond end of map");
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
FILE * in_file = NULL;


int main()
{
    //needs to be readable because after mmap'ing, whole pages must be READ into memory before individual items can be written
    fd = open(filename, O_CREAT|O_RDWR, S_IRUSR | S_IWUSR);  
    
    struct stat stats;
    if (0 != fstat(fd, &stats)) { perror ("fstat"); exit(0); }

    if (stats.st_size == 0)
    {
        size_t ps = sysconf (_SC_PAGESIZE);
        if (0 !=ftruncate(fd, ps))
            {perror("ftruncate"); exit(0); }
        if (0 != fstat(fd, &stats)) { perror ("fstat"); exit(0); }
    }
    
    file_no_nodes = stats.st_size / 8;
    assert( stats.st_size % 4096 == 0);
    //WARNING: file is going to be mapped write-only!!!
    map = (int32_t*)mmap(NULL, stats.st_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); exit(0);}

    int in_fd = 0; //default to standard input
    //in_fd = open("isle_of_man.osm", O_RDONLY);
    in_file = fdopen(in_fd, "r"); 

    std::string line;
    int64_t nNodes = 0;
    int64_t nWays = 0;
    int64_t nRelations = 0;
    //const char* c_line;
    std::list<int64_t> node_refs;
    
    char* line_buffer = NULL;
    size_t line_buffer_size = 0;
    //while (readLine(&c_line))
    //!in.eof())
    while (0 <= getline(&line_buffer, &line_buffer_size, in_file))
    {   
    
        /*line_length = ;
        if (line_length < 1)
            {perror("getline"); exit(0);}*/
        
        //getline(in, line);
        //std::string line(line_buffer);
        //std::cout << line << std::endl;
//        if (line.length() < 4) assert(false && "Unexpected empty line");
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
            
            //degToInt(std::string(getValue(line_buffer, "lat")));
            int32_t lon = degValueToInt(line_buffer, "lon");
            //degToInt(std::string(getValue(line_buffer, "lon")));
            //TODO: change atoi to strtoul, since node ids may be longer than 32 bits
            int32_t id =  atoi(getValue(line_buffer, "id"));
            //std::cout << line << std::endl;
            //std::cout << id << ": (" << lat << ", " << lon << ")" << std::endl;
            dumpNode(id, lat, lon);
#endif
        } else if (strstr(line_buffer, "<way"))
        // (line.find("<way") != std::string::npos)
        {
            nWays++;
            if (nWays % 1000000 == 0)
                std::cout << (nWays/1000000) << "M ways processed" << std::endl;
            node_refs.clear();
        }
        else if (strstr(line_buffer, "<relation"))
        //(line.find("<relation") != std::string::npos)
        {
            nRelations++;
            if (nRelations % 1000000 == 0)
                std::cout << (nRelations/1000000) << "M relations processed" << std::endl;
            
            node_refs.clear();
        }
        else if (strstr(line_buffer, "<nd"))
        //(line.find("<nd") != std::string::npos)
        {
            int64_t ref = strtoul( getValue(line_buffer, "ref"), NULL, 10);
            node_refs.push_back( ref);
        }
        else if (strstr(line_buffer, "<tag"))
        //line.find("<tag") != std::string::npos)
        {
            std::string key = getValue(line_buffer, "k");
            std::string val = getValue(line_buffer, "v");
            //if ((key == "admin_level") && (val == "2"))
            if ((key == "natural") && (val == "coastline"))
            {
                print_nodes(node_refs);
            }
            //std::cerr << key << "\t" << val << std::endl;
        }
    } //while (line_buffer[line_length-1] == '\n');
    free (line_buffer);
    
    std::cout << "statistics\n==========" << std::endl;
    std::cout << "#nodes: " << nNodes << std::endl;
    std::cout << "#ways: " << nWays << std::endl;
    std::cout << "#relations: " << nRelations << std::endl;
	return 0;
}


