
#include "osm_types.h"

#include <map>
#include <string.h>
#include <assert.h>

#include <fcntl.h>
#include <stdio.h>


/** on-file layout for tags :
    uint16_t num_symbolic_tags;
    uint16_t num_verbose_tags;
    uint8_t symbolic_tags[num_symbolic_tags];
    uint64_t verbose_tag_data_len; (total data size of all verbose tags, in bytes); optional, present only if num_verbose_tags > 0
    <2*num_verbose_tags> zero-terminated strings containing interleaved keys and values 
    (key_0, value_0, key_1, value_1, ...)

*/
void serializeTags( const list<OSMKeyValuePair> tags, FILE* file, const map<OSMKeyValuePair, uint8_t> &tag_symbols)
{
    list<OSMKeyValuePair> verbose_tags;
    list<uint8_t> symbolic_tags;
    
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
    
    for (list<uint8_t>::const_iterator it = symbolic_tags.begin(); it!= symbolic_tags.end(); it++)
    {
        uint8_t tmp = *it;
        fwrite( &tmp, sizeof(tmp), 1, file);
    }
    
    if (num_verbose_tags == 0) return;
    assert(verbose_tag_data_len < 1ll<<32);
    uint32_t vtdl = verbose_tag_data_len;
    fwrite( &vtdl, sizeof(vtdl), 1, file);
    for (list<OSMKeyValuePair>::const_iterator it = verbose_tags.begin(); it!= verbose_tags.end(); it++)
    {
        fwrite( it->first.c_str(),  strlen(it->first.c_str()) + 1, 1, file);    //both including their null-termination
        fwrite( it->second.c_str(), strlen(it->second.c_str())+ 1, 1, file);
    }
}

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

void OSMRelation::serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const
{
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped way *starts*
    
    assert(members.size() < (1<<16));
    uint32_t num_members = members.size();
    fwrite(&num_members, sizeof(num_members), 1, data_file);
    
    uint32_t members_data_size = 0;
    for (list<OSMRelationMember>::const_iterator it = members.begin(); it != members.end(); it++)
        members_data_size += it->getDataSize();
    
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

uint32_t OSMRelationMember::getDataSize() const 
{ 
    return sizeof(ELEMENT) + sizeof(uint64_t) + strlen(role.c_str()) + 1; //1 byte for NULL-termination
} 

