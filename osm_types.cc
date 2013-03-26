

#include <map>
#include <ostream>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> //for exit()
#include <stdint.h>

#include <boost/foreach.hpp>

#include "osm_types.h"
#include "symbolic_tags.h"

#include <iostream>

std::ostream& operator <<(std::ostream& os, const OSMVertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}


/** on-file layout for tags :
    uint16_t num_symbolic_tags;
    uint16_t num_verbose_tags;  //TODO: are these really necessary?
    uint8_t symbolic_tags[num_symbolic_tags];
    uint32_t verbose_tag_data_len; (total data size of all verbose tags, in bytes); optional, present only if num_verbose_tags > 0
    <2*num_verbose_tags> zero-terminated strings containing interleaved keys and values 
    (key_0, value_0, key_1, value_1, ...)

*/
void serializeTags( const list<OSMKeyValuePair> tags, FILE* file, const map<OSMKeyValuePair, uint8_t> *tag_symbols)
{
    list<OSMKeyValuePair> verbose_tags;
    list<uint8_t> symbolic_tags;
    
    if (tag_symbols)
    {
        for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        {
            if ( tag_symbols->count(*it)) 
                symbolic_tags.push_back( tag_symbols->find(*it)->second);
            else 
                verbose_tags.push_back(*it);
        }
    } else verbose_tags = tags; //if there are no symbolic tags, everything is verbose
    
    assert(symbolic_tags.size() < (1<<16));
    uint16_t num_symbolic_tags = symbolic_tags.size();
    fwrite(&num_symbolic_tags, sizeof(num_symbolic_tags), 1, file);
    
    assert(verbose_tags.size() < (1<<16));
    uint16_t num_verbose_tags = verbose_tags.size();
    fwrite(&num_verbose_tags, sizeof(num_verbose_tags), 1, file);
    
    for (list<uint8_t>::const_iterator it = symbolic_tags.begin(); it!= symbolic_tags.end(); it++)
    {
        uint8_t tmp = *it;
        fwrite( &tmp, sizeof(tmp), 1, file);
    }
    
    if (num_verbose_tags == 0) return;

    for (list<OSMKeyValuePair>::const_iterator it = verbose_tags.begin(); it!= verbose_tags.end(); it++)
    {
        fwrite( it->first.c_str(),  strlen(it->first.c_str()) + 1, 1, file);    //both including their null-termination
        fwrite( it->second.c_str(), strlen(it->second.c_str())+ 1, 1, file);
    }
}

void fread( void* dest, uint64_t size, FILE* file)
{
    //FIXME: switch "1" and "size"
    uint64_t num_read = fread(dest, 1, size, file);
    assert(num_read == size);
    if (num_read < 1)
    { perror("[ERR] fread"); exit(0);}
}

list<OSMKeyValuePair> deserializeTags(const uint8_t* &data_ptr)
{
    list<OSMKeyValuePair> tags;
    
    
    uint16_t num_symbolic_tags = *((const uint16_t*)data_ptr);
    data_ptr+=2;
    uint16_t num_verbose_tags  = *((const uint16_t*)data_ptr);
    data_ptr+=2;
    for (int i = 0; i < num_symbolic_tags; i++)
    {
        uint8_t tag_id = *data_ptr++;
        tags.push_back( OSMKeyValuePair( symbolic_tags_keys[tag_id] , symbolic_tags_values[tag_id] ));
    }
    if (num_verbose_tags == 0) return tags;
    
    //const char* str = (const char*)data_ptr;
    while (num_verbose_tags--)
    {
        const char* key = (const char*)data_ptr;
        data_ptr += strlen( (const char*)data_ptr)+1;
        tags.push_back( OSMKeyValuePair( key, (const char*)data_ptr));
        data_ptr += strlen( (const char*)data_ptr)+1;
    }
    
    return tags;
    
}

static const char* freadstr(FILE *src)
{
    static char* buf = (char*) malloc(1);
    static uint32_t buf_size = 1;

    uint32_t idx = 0;
    do
    {
        if (idx >= buf_size)
        {
            buf = (char*)realloc(buf, buf_size*2);
            buf_size = buf_size*2;
        }
        buf[idx] = fgetc(src);
        if (buf[idx] == '\0') return buf;
        idx++;
    } while (!feof(src));
    return buf;

}

list<OSMKeyValuePair> deserializeTags(FILE* src)
{
    list<OSMKeyValuePair> tags;
    
    uint16_t num_symbolic_tags;
    fread( &num_symbolic_tags, sizeof(num_symbolic_tags), src);
    uint16_t num_verbose_tags;
    fread( &num_verbose_tags, sizeof(num_verbose_tags), src);
    for (int i = 0; i < num_symbolic_tags; i++)
    {
        uint8_t tag_id;
        fread( &tag_id, sizeof(tag_id), src);
        tags.push_back( OSMKeyValuePair( symbolic_tags_keys[tag_id] , symbolic_tags_values[tag_id] ));
    }
    //if (num_verbose_tags == 0) return tags;
    
    while (num_verbose_tags--)
    {
        string key = freadstr(src);
        string value = freadstr(src);
        tags.push_back( OSMKeyValuePair( key, value));
    }
    
    return tags;
    
}

/*
list<OSMKeyValuePair> deserializeTags(FILE* data_file, uint64_t file_offset)
{
    fseeko(data_file, file_offset, SEEK_SET);
    return deserializeTags(data_file);
}*/

ostream& operator<<(ostream &out, const list<OSMKeyValuePair> &tags)
{
    out << "[";
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
    {
        out << "\"" << it->first << "\" = \"" << it->second << "\"";
        if (++it != tags.end()) out << ", ";
        it--;
    }
    out << "]";
    return out;
}

/*
OSMNode::OSMNode( FILE* data_file, uint64_t offset, uint64_t node_id)
{
    fseeko(data_file, offset, SEEK_SET);
    fread(&lat, sizeof(lat), data_file);
    fread(&lon, sizeof(lon), data_file);
    tags = deserializeTags(data_file);
    id = node_id;
}*/

OSMNode::OSMNode( const uint8_t* data_ptr, uint64_t node_id)
{
    lat = *(int32_t*)data_ptr;
    data_ptr+=4;
    lon = *(int32_t*)data_ptr;
    data_ptr+=4;
    tags = deserializeTags(data_ptr);
    id = node_id;
}

OSMNode::OSMNode( int32_t node_lat, int32_t node_lon, uint64_t  node_id, list<OSMKeyValuePair> node_tags):
        lat(node_lat), lon(node_lon), id(node_id), tags(node_tags) {}

void OSMNode::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> *tag_symbols) const
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped node *starts*
    fwrite( &lat, sizeof(lat), 1, data_file);
    fwrite( &lon, sizeof(lon), 1, data_file);

    serializeTags( tags, data_file, tag_symbols );

    //std::cout << id << endl;    
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
}

bool OSMNode::operator==(const OSMNode &other) const {return lat == other.lat && lon == other.lon;}
bool OSMNode::operator!=(const OSMNode &other) const {return lat != other.lat || lon != other.lon;}
bool OSMNode::operator< (const OSMNode &other) const {return id < other.id;}

ostream& operator<<(ostream &out, const OSMNode &node)
{
    out << "Node " << node.id <<" (" << (node.lat/10000000.0) << "°, " << (node.lon/10000000.0) << "°)";
    out << node.tags;
    return out;
}

OSMWay::OSMWay( uint64_t way_id, list<uint64_t> way_refs, list<OSMKeyValuePair> way_tags):
        id(way_id), refs(way_refs), tags(way_tags) {}

OSMWay::OSMWay( const uint8_t* data_ptr, uint64_t way_id): id(way_id)
{
    uint32_t num_node_refs = *(uint32_t*)data_ptr;
    data_ptr+=4;
    while (num_node_refs--)
    {
        refs.push_back( *(uint64_t*)data_ptr);
        data_ptr+=8;
    }
    tags = deserializeTags(data_ptr);
}

        

void OSMWay::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> *tag_symbols) const
{
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped way *starts*
    
    uint32_t num_node_refs = refs.size();
    fwrite(&num_node_refs, sizeof(num_node_refs), 1, data_file);
    
    for (list<uint64_t>::const_iterator it = refs.begin(); it != refs.end(); it++)
    {
        uint64_t ref = *it;
        fwrite(&ref, sizeof(ref), 1, data_file);
    }
    
    serializeTags(tags, data_file, tag_symbols);
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
}

bool OSMWay::hasKey(string key) const
{
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return true;
    return false;
}

const string& OSMWay::getValue(string key) const
{
    static const string empty="";
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return it->second;
    return empty;
}

ostream& operator<<(ostream &out, const OSMWay &way)
{
    out << "Way " << way.id << " (";
    for (list<uint64_t>::const_iterator it = way.refs.begin(); it!= way.refs.end(); it++)
    {
        out << *it;
        if (++it != way.refs.end()) out << ", ";
        it--;
    }
    out << ") " << way.tags;
    return out;
}

//===================================

OSMIntegratedWay::OSMIntegratedWay( uint64_t way_id, list<OSMVertex> way_vertices, list<OSMKeyValuePair> way_tags):
        id(way_id), vertices(way_vertices), tags(way_tags) {}

OSMIntegratedWay::OSMIntegratedWay( const uint8_t* &data_ptr, uint64_t way_id): id(way_id)
{
    uint32_t num_vertices = *(uint32_t*)data_ptr;
    data_ptr+=4;
    while (num_vertices--)
    {
        int32_t lat = *(int32_t*)data_ptr;
        data_ptr+=4;
        int32_t lon = *(int32_t*)data_ptr;
        data_ptr+=4;
        vertices.push_back( OSMVertex(lat, lon));
    }
    tags = deserializeTags(data_ptr);
}

OSMIntegratedWay::OSMIntegratedWay( FILE* src, uint64_t way_id): id(way_id)
{
    uint32_t num_vertices;// = vertices.size();
    fread(&num_vertices, sizeof(num_vertices), src);
    while (num_vertices--)
    {
        int32_t lat;
        size_t num_read = fread(&lat, sizeof(lat), 1, src);
        if (num_read != 1)
        {
            cout << "Invalid read operation" << endl;
            exit(0);
        }
        
        int32_t lon;
        num_read = fread(&lon, sizeof(lon), 1, src);
        if (num_read != 1)
        {
            cout << "Invalid read operation" << endl;
            exit(0);
        }

        vertices.push_back( OSMVertex(lat, lon));
    }
    tags = deserializeTags(src);
}


void OSMIntegratedWay::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> *tag_symbols) const
{
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped way *starts*
    
    uint32_t num_vertices = vertices.size();
    fwrite(&num_vertices, sizeof(num_vertices), 1, data_file);
    
    for (list<OSMVertex>::const_iterator it = vertices.begin(); it != vertices.end(); it++)
    {
        int32_t lat = it->x;
        fwrite(&lat, sizeof(lat), 1, data_file);
        int32_t lon = it->y;
        fwrite(&lon, sizeof(lon), 1, data_file);
    }
    
    serializeTags(tags, data_file, tag_symbols);
    
    if (index_map)
    {
        ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
        uint64_t* ptr = (uint64_t*)index_map->ptr;
        assert (ptr[id] == 0 || ptr[id] == offset);
        ptr[id] = offset;
    }
}

bool OSMIntegratedWay::hasKey(string key) const
{
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return true;
    return false;
}

const string& OSMIntegratedWay::getValue(string key) const
{
    static const string empty="";
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return it->second;
    return empty;
}


ostream& operator<<(ostream &out, const OSMIntegratedWay &way)
{
    out << "Way " << way.id << " (";
    for (list<OSMVertex>::const_iterator it = way.vertices.begin(); it!= way.vertices.end(); it++)
    {
        OSMVertex v = *it;
        out << v;
        if (++it != way.vertices.end()) out << ", ";
        it--;
    }
    out << ") " << way.tags;
    return out;
}

//==================================

OSMRelation::OSMRelation( uint64_t relation_id, list<OSMRelationMember> relation_members, list<OSMKeyValuePair> relation_tags):
    id(relation_id), members(relation_members), tags(relation_tags) {}

OSMRelation::OSMRelation( const uint8_t* data_ptr, uint64_t relation_id): id(relation_id)
{
    uint32_t num_members = *(uint32_t*)data_ptr;
    data_ptr+=4;

    //uint32_t members_data_size = *(uint32_t)data_ptr; //don't need these, but their are still present in the data file
    //data_ptr+=4;
    
    while (num_members--)
    {
        ELEMENT type = *(ELEMENT*)data_ptr;
        data_ptr+=sizeof(ELEMENT);
        uint64_t ref = *(uint64_t*)data_ptr;
        data_ptr+=sizeof(uint64_t);
        const char* role = (const char*)data_ptr;
        data_ptr+=strlen(role)+1;  //including zero termination
        members.push_back( OSMRelationMember(type, ref, role));
    }
   
    tags = deserializeTags(data_ptr);
}

OSMRelation::OSMRelation( FILE* src, uint64_t rel_id): id(rel_id)
{
    uint32_t num_members;
    if (1 != fread(&num_members, sizeof(num_members), 1, src)) return;
    
    while (num_members--)
    {
        ELEMENT type;
        if (1 != fread (&type, sizeof(ELEMENT), 1, src)) return;
        
        uint64_t ref;
        if (1 != fread (&ref, sizeof(ref), 1, src)) return;
        
        int i;
        string s;
        while ( (i = fgetc(src)) != EOF  && i != '\0')
            s+=i;
        //data_ptr+=strlen(role)+1;  //including zero termination
        members.push_back( OSMRelationMember(type, ref, s));
    }
   
    tags = deserializeTags(src);
}

void OSMRelation::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> *tag_symbols) const
{
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped way *starts*
    
    assert(members.size() < (1<<16));
    uint32_t num_members = members.size();
    fwrite(&num_members, sizeof(num_members), 1, data_file);
    
    /*uint32_t members_data_size = 0;
    for (list<OSMRelationMember>::const_iterator it = members.begin(); it != members.end(); it++)
        members_data_size += it->getDataSize();
    */
    for (list<OSMRelationMember>::const_iterator it = members.begin(); it != members.end(); it++)
    {
        ELEMENT type = it->type;
        fwrite(&type, sizeof(type), 1, data_file);
        
        uint64_t ref = it->ref;
        fwrite(&ref, sizeof(ref), 1, data_file);
        
        const char* str = it->role.c_str();
        fwrite(str, strlen(str)+1, 1, data_file);
    }
        
    
//    list<OSMRelationMember> members;

    serializeTags(tags, data_file, tag_symbols);
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
    
}

bool OSMRelation::hasKey(string key) const
{
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return true;
    return false;
}

const string& OSMRelation::getValue(string key) const
{
    static const string empty = "";
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return it->second;
    return empty;
}

ostream& operator<<(ostream &out, const OSMRelation &relation)
{
    static const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};
    out << "Relation " << relation.id << " ";
    for (list<OSMRelationMember>::const_iterator it = relation.members.begin(); it!= relation.members.end(); it++)
    {
        out << " (" << it->role << " " <<ELEMENT_NAMES[it->type] << " " << it->ref << ")" ;
    }
    
    out << " "<< relation.tags;
    return out;
}

uint32_t OSMRelationMember::getDataSize() const 
{ 
    return sizeof(ELEMENT) + sizeof(uint64_t) + strlen(role.c_str()) + 1; //1 byte for NULL-termination
} 

