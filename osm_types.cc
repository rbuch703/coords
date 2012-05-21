

#include <map>
#include <ostream>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> //for exit()

#include "osm_types.h"
#include "symbolic_tags.h"


/** on-file layout for tags :
    uint16_t num_symbolic_tags;
    uint16_t num_verbose_tags;  //TODO: are these really necessary?
    uint8_t symbolic_tags[num_symbolic_tags];
    uint32_t verbose_tag_data_len; (total data size of all verbose tags, in bytes); optional, present only if num_verbose_tags > 0
    <2*num_verbose_tags> zero-terminated strings containing interleaved keys and values 
    (key_0, value_0, key_1, value_1, ...)

*/
void serializeTags( const list<OSMKeyValuePair> tags, FILE* file, const map<OSMKeyValuePair, uint8_t> &tag_symbols)
{
    list<OSMKeyValuePair> verbose_tags;
    list<uint8_t> symbolic_tags;
    
    uint32_t verbose_tag_data_len = 0;
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
    
    for (list<uint8_t>::const_iterator it = symbolic_tags.begin(); it!= symbolic_tags.end(); it++)
    {
        uint8_t tmp = *it;
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

void fread( void* dest, uint64_t size, FILE* file)
{
    if (size != fread(dest, size, 1, file))
    { perror("[ERR] fread"); exit(0);}
}


list<OSMKeyValuePair> deserializeTags(FILE* data_file)
{
    list<OSMKeyValuePair> tags;
    
    
    uint16_t num_symbolic_tags;
    fread(&num_symbolic_tags, sizeof(num_symbolic_tags), data_file);
    
    uint16_t num_verbose_tags;  //TODO: are these really necessary? --> yes, if it is zero, then this is the final element
    fread(&num_verbose_tags, sizeof(num_verbose_tags), data_file);
    
    for (int i = 0; i < num_symbolic_tags; i++)
    {
        uint8_t tag_id;
        fread(&tag_id, sizeof(tag_id), data_file);
        tags.push_back( OSMKeyValuePair( symbolic_tags_keys[tag_id] , symbolic_tags_values[tag_id] ));
    }
    if (num_verbose_tags == 0) return tags;
    
    uint32_t verbose_tag_data_len;
    fread(&verbose_tag_data_len, sizeof(verbose_tag_data_len), data_file);
    
    //TODO: maybe replace this with a static buffer to reduce number of reallocations;
    // however, this would mean that the method cannot be used in multithread-applications
    char *buf = new char[verbose_tag_data_len];
    const char* str = buf;
    while (num_verbose_tags--)
    {
        const char* key = str;
        str += strlen(str)+1;
        tags.push_back( OSMKeyValuePair( key, str));
        str += strlen(str)+1;
    }
    
    
    delete [] buf;
    return tags;
}

list<OSMKeyValuePair> deserializeTags(const uint8_t* data_ptr)
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
    
    //seems we don't needs this after all when working with memory maps, but it is still stored in the data file
    //uint32_t verbose_tag_data_len = *((const uint32_t*)data_ptr); 
    data_ptr+=4;

    const char* str = (const char*)data_ptr;
    while (num_verbose_tags--)
    {
        const char* key = str;
        str += strlen(str)+1;
        tags.push_back( OSMKeyValuePair( key, str));
        str += strlen(str)+1;
    }
    
    return tags;
    
}


list<OSMKeyValuePair> deserializeTags(FILE* data_file, uint64_t file_offset)
{
    fseeko(data_file, file_offset, SEEK_SET);
    return deserializeTags(data_file);
}

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

OSMNode::OSMNode( FILE* data_file, uint64_t offset, uint64_t node_id)
{
    fseeko(data_file, offset, SEEK_SET);
    fread(&lat, sizeof(lat), data_file);
    fread(&lon, sizeof(lon), data_file);
    tags = deserializeTags(data_file);
    id = node_id;
}

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

void OSMNode::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped node *starts*
    fwrite( &lat, sizeof(lat), 1, data_file);
    fwrite( &lon, sizeof(lon), 1, data_file);

    //map<OSMKeyValuePair, uint16_t> dummy; //empty dummy map TODO: replace by actual map of most-frequently used kv-pairs
    serializeTags( tags, data_file, tag_symbols );
    
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

        

void OSMWay::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const
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

string OSMWay::getValue(string key) const
{
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return it->second;
    return "";
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

void OSMRelation::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const
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

string OSMRelation::getValue(string key) const
{
    for (list<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
        if (it->first == key) return it->second;
    return "";
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

