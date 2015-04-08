

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <iostream>

#include "osm/osmParserXml.h"

using namespace std;

static const char* getValue(const char* line, const char* key)
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
static int32_t degValueToInt(const char* line, const char* key)
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

OsmXmlParser::OsmXmlParser(FILE * file, OsmBaseConsumer *consumer): 
    consumer(consumer), in_file(file), line_buffer(NULL), line_buffer_size(0), numLinesRead(0), 
    hasParsedNodes(false), hasParsedWays(false), hasParsedRelations(false) { }

OsmXmlParser::~OsmXmlParser() { free (line_buffer); }


void OsmXmlParser::parse()
{
    while (readNextLine())
    {   
        
        //TODO: replace strstr(line_buffer, ...) by strcmp(line, ...)
        
        if      (strncmp(line, "<changeset" ,10) == 0) { parseChangeset(); }
        else if (strncmp(line, "<node"      , 5) == 0) { parseNode();}
        else if (strncmp(line, "<way"       , 4) == 0) { parseWay();}
        else if (strncmp(line, "<relation"  , 9) == 0) { parseRelation();}

        
        else if (strncmp(line, "<?xml ", 6) == 0) { continue;}
        else if (strncmp(line, "<osm ",  5) == 0) { continue;}
        else if (strncmp(line, "</osm>",  6) == 0) { continue;}
        //else if (strstr(line_buffer, "</")) { continue;}
        //FIXME: assumes that exactly the remainder of the current line is a comment
        else if (strncmp(line, "<!--",   4) == 0) { continue;} 
        else if (strncmp(line, "<bound", 6) == 0) { continue;}
        else
        {
            printf("[ERROR] unknown tag in %s\n", line);
            assert(false);
            exit(0);
        }
    }
}

void OsmXmlParser::parseNode()
{
//    if (!hasParsedNodes) { hasParsedNodes = true; consumer->beforeParsingNodes();}
    int32_t lat = degValueToInt(line, "lat"); 
    int32_t lon = degValueToInt(line, "lon");
    uint64_t id = strtoull(getValue(line, "id"), NULL, 10);
    uint32_t version = atoi(getValue(line, "version"));
    vector<OsmKeyValuePair> tags;
    if (!strstr(line, "/>")) //node contains tags (kv-pairs)
    {
        while (readNextLine())
        {
            if (strncmp(line, "</node", 6) == 0) break;
            assert( strncmp(line, "<tag", 4) == 0 && "Unknown tag in node");
            string key = getValue(line, "k");
            string val = getValue(line, "v");
            tags.push_back( OsmKeyValuePair(key, val));
        }
    }
    OsmNode node(lat, lon, id, version, tags);
    consumer->consumeNode(node);
}

void OsmXmlParser::parseWay()
{
//    if (!hasParsedWays) { 
//        hasParsedWays = true; 
//        consumer->afterParsingNodes(); 
//        consumer->beforeParsingWays();
//    }
    int64_t id = strtoul(getValue(line, "id"), NULL, 10);
    uint32_t version = strtoul(getValue(line, "version"), NULL, 10);

    vector<OsmKeyValuePair> tags;
    vector<uint64_t> node_refs;
    if (strstr(line, "/>")) //way without node references???
    {
        cout << "[WARN] way " << id << " has no node references" << endl;
        return; //skip this way
    }
    
    while (readNextLine())
    {
        if (strncmp(line, "</way", 5) == 0) break;
        
        if (strncmp(line, "<nd", 3) == 0) // node reference
            node_refs.push_back( strtoul( getValue(line, "ref"), NULL, 10));
        else if (strncmp(line, "<tag", 4) == 0)
        {
            string key = getValue(line, "k");
            string val = getValue(line, "v");
            tags.push_back( OsmKeyValuePair(key, val));
        } else assert( false && "Unknown tag in Way" );
    }
    
    if (node_refs.size() == 0)
    {
        cout << "[WARN] way " << id << " has no node references" << endl;
        return; //skip this way
    }
    
    OsmWay way(id, version, node_refs, tags);
    consumer->consumeWay(way);
}

void OsmXmlParser::parseRelation()
{
/*    if (!hasParsedRelations) { 
        hasParsedRelations = true; 
        consumer->afterParsingWays(); 
        consumer->beforeParsingRelations();
    }
    */
    int64_t id       = strtoul(getValue(line, "id"),      NULL, 10);
    uint32_t version = strtoul(getValue(line, "version"), NULL, 10);
    vector<OsmKeyValuePair> tags;
    vector<OsmRelationMember> members;
    
    if (strstr(line, "/>")) //a relation needs at least one member 
    {
        cout << "[WARN] relation " << id << " has no node/way/relation references" << endl;
        return; //skip this relation
    }
    
    while (readNextLine())
    {
        if (strncmp(line, "</relation", 10) == 0) break;
        
        if (strncmp(line, "<member", 7) == 0) 
        {
            const char* type_str = getValue(line, "type");
            ELEMENT type;
            if      (strcmp(type_str, "node")     == 0 ) type = NODE;
            else if (strcmp(type_str, "way")      == 0 ) type = WAY;
            else if (strcmp(type_str, "relation") == 0 ) type = RELATION;
            else {printf("[ERR] relation member with unknown type '%s'\n", type_str); continue;}
            
            uint64_t ref = strtoul( getValue(line, "ref"), NULL, 10);
            string role = getValue(line, "role");
            
            members.push_back (OsmRelationMember(type, ref, role));
        }
        else if (strncmp(line, "<tag", 4) == 0)
        {
            string key = getValue(line, "k");
            string val = getValue(line, "v");
            tags.push_back( OsmKeyValuePair(key, val));
        } else assert(false && "Unknown tag in relation" );
    }
    OsmRelation rel(id, version, members, tags);
    consumer->consumeRelation(rel);
}

void OsmXmlParser::parseChangeset()
{
    //right now, we don't care about changesets, so we just make sure to correctly skip over them
    
    if (strstr(line, "/>")) return; //single-line changeset
    
    while (readNextLine())
    {
        if (strncmp(line, "</changeset>", 12) == 0) break;
        if (strncmp(line, "<tag", 4) != 0)
        {
            cout << "Unterminated changeset in '" << line << "'" << endl;
            assert(false);
        }
    }
}   

bool OsmXmlParser::readNextLine() { 
    
    if (getline(&line_buffer, &line_buffer_size, in_file) == -1) return false;
    if (++numLinesRead % 10000000 == 0) cout << numLinesRead/1000000 << "M lines read" << endl;
    line = line_buffer;
    while (*line == ' ' || *line == '\t') line++;

    return true;
}

