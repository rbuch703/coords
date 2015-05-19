

#include <map>
#include <ostream>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> //for exit()
#include <stdint.h>

#include "osmTypes.h"
#include "misc/rawTags.h"
//#include "symbolic_tags.h"

#include <iostream>

using std::vector;
using std::ostream;
using std::string;
//using namespace std;

ostream& operator<<(ostream &out, const vector<OsmKeyValuePair> &tags)
{
    out << "[";
    for (vector<OsmKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
    {
        out << "\"" << it->first << "\" = \"" << it->second << "\"";
        if (++it != tags.end()) out << ", ";
        it--;
    }
    out << "]";
    return out;
}


OsmNode::OsmNode( const uint8_t* data_ptr)
{
    id = *(uint64_t*)data_ptr;
    data_ptr += sizeof(uint64_t);

    version = *(uint32_t*)data_ptr;
    data_ptr+= sizeof(uint32_t);
    
    lat = *(int32_t*)data_ptr;
    data_ptr+=4;

    lon = *(int32_t*)data_ptr;
    data_ptr+=4;

    RawTags rawTags(data_ptr);
    
    //data_ptr += rawTags.getSerializedSize();
    
    for (std::pair<const char*, const char*> kv : rawTags)
        tags.push_back( std::make_pair(kv.first, kv.second));
}

uint64_t OsmNode::getSerializedSize() const
{
    return sizeof(id) + sizeof(version) + sizeof(lat) + sizeof(lon) + RawTags::getSerializedSize(tags);
}

OsmNode::OsmNode( int32_t lat, int32_t lon, uint64_t  id, uint32_t version, vector<OsmKeyValuePair> tags): id(id), version(version), lat(lat), lon(lon), tags(tags) {}

void OsmNode::serializeWithIndexUpdate( FILE* dataFile, mmap_t *index_map) const
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    assert (id > 0);  

    uint64_t offset = ftello(dataFile);    //get offset at which the dumped node *starts*
    fwrite( &id,      sizeof(id),      1, dataFile);
    fwrite( &version, sizeof(version), 1, dataFile);
    fwrite( &lat,     sizeof(lat),     1, dataFile);
    fwrite( &lon,     sizeof(lon),     1, dataFile);

    RawTags::serialize(tags, dataFile);

    //std::cout << id << endl;    
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
}

void OsmNode::serializeWithIndexUpdate( ChunkedFile& dataFile, mmap_t *index_map) const
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    MUST (id > 0, "Invalid non-positive node id in input file.");
    
    Chunk chunk = dataFile.createChunk( this->getSerializedSize());
    chunk.put(id);
    chunk.put(version);
    chunk.put(lat);
    chunk.put(lon);

    RawTags::serialize( tags, chunk );

    //std::cout << id << endl;    
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = chunk.getPositionInFile();
}


bool OsmNode::hasKey(string key) const
{
    for (const OsmKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OsmNode::getValue(string key) const
{
    static const string empty="";
    for (const OsmKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

    return empty;
}


bool OsmNode::operator==(const OsmNode &other) const {return lat == other.lat && lon == other.lon;}
bool OsmNode::operator!=(const OsmNode &other) const {return lat != other.lat || lon != other.lon;}
bool OsmNode::operator< (const OsmNode &other) const {return id < other.id;}

ostream& operator<<(ostream &out, const OsmNode &node)
{
    out << "Node " << node.id <<" (" << (node.lat/10000000.0) << "°, " << (node.lon/10000000.0) << "°)";
    out << node.tags;
    return out;
}

OsmWay::OsmWay( uint64_t id, uint32_t version, 
            std::vector<uint64_t> way_refs, std::vector<OsmKeyValuePair> tags):
        id(id), version(version), tags(tags)  
{ 
    for (uint64_t ref : way_refs)
        refs.push_back( (OsmGeoPosition){.id = ref, .lat=INVALID_LAT_LNG, .lng = INVALID_LAT_LNG} );
}

OsmWay::OsmWay( uint64_t id, uint32_t version, 
                std::vector<OsmGeoPosition> refs, 
                std::vector<OsmKeyValuePair> tags):
                id(id), version(version), refs(refs), tags(tags) { }


OsmWay::OsmWay( const uint8_t* data_ptr)
{
    this->id = *(uint64_t*)data_ptr;
    data_ptr += sizeof( uint64_t );
    
    this->version = *(uint32_t*)data_ptr;
    data_ptr += sizeof( uint32_t );
    
    uint16_t num_node_refs = *(uint16_t*)data_ptr;
    data_ptr+= sizeof(uint16_t);
    
    while (num_node_refs--)
    {
        refs.push_back( (OsmGeoPosition){ .id = *(uint64_t*)data_ptr, 
                                          .lat = ((int32_t*)data_ptr)[2],
                                          .lng = ((int32_t*)data_ptr)[3]} );
        data_ptr+= (sizeof(uint64_t) + 2* sizeof(int32_t));
    }
    
    RawTags rawTags(data_ptr);
    
    for ( const std::pair< const char*, const char*> &kv : rawTags)
        tags.push_back( std::make_pair(kv.first, kv.second));
        
}

uint64_t OsmWay::getSerializedSize() const
{
    return sizeof(uint64_t) + //id
           sizeof(uint32_t) + //version
           sizeof(uint16_t) + // numRefs
           sizeof(OsmGeoPosition) * refs.size() + //node refs
           RawTags::getSerializedSize(tags);
}

        

void OsmWay::serialize( FILE* data_file, mmap_t *index_map) const
{
    assert (id > 0);  
    //get offset at which the dumped way *starts*
    uint64_t offset = index_map ? ftello(data_file) : 0;

    MUST( 1 == fwrite(&this->id, sizeof(this->id), 1, data_file), "write error");
    MUST( 1 == fwrite(&this->version, sizeof(this->version), 1, data_file), "write error");

    
    MUST(refs.size() <= 2000, "#refs in way is beyond what's allowed by OSM spec");
    uint16_t num_node_refs = refs.size();
    fwrite(&num_node_refs, sizeof(num_node_refs), 1, data_file);

    assert(sizeof(OsmGeoPosition) == sizeof(uint64_t) + 2* sizeof(uint32_t));
    
    for (OsmGeoPosition pos : refs)
        fwrite(&pos, sizeof(pos), 1, data_file);
    
    RawTags::serialize(tags, data_file);

    if (index_map)
    {
        ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
        uint64_t* ptr = (uint64_t*)index_map->ptr;
        ptr[id] = offset;
    }
}

void OsmWay::serialize( ChunkedFile& dataFile, mmap_t *index_map) const
{
    Chunk chunk = dataFile.createChunk(this->getSerializedSize());
    chunk.put<uint64_t>(this->id);
    chunk.put<uint32_t>(this->version);
    MUST(refs.size() <= 2000, "#refs in way is beyond what's allowed by OSM spec");
    chunk.put<uint16_t>(this->refs.size());
    for (const OsmGeoPosition &pos: this->refs)
        chunk.put(&pos, sizeof(OsmGeoPosition));

    RawTags::serialize(this->tags, chunk);
    if (index_map)
    {
        ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
        uint64_t* ptr = (uint64_t*)index_map->ptr;
        ptr[id] = chunk.getPositionInFile();
    }
    
}


bool OsmWay::hasKey(string key) const
{
    for (const OsmKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OsmWay::getValue(string key) const
{
    static const string empty="";
    for (const OsmKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

    return empty;
}

ostream& operator<<(ostream &out, const OsmWay &way)
{
    out << "Way " << way.id << " (";
    for ( OsmGeoPosition pos : way.refs)
        out << pos.id << ", ";

    out << ") " << way.tags;
    return out;
}



//==================================

OsmRelation::OsmRelation( uint64_t id, uint32_t version, vector<OsmRelationMember> members, vector<OsmKeyValuePair> tags): id(id), version(version), members(members), tags(tags) {}

OsmRelation::OsmRelation( const uint8_t* data_ptr)
{
    id = *(uint64_t*)data_ptr;
    data_ptr += sizeof(uint64_t);
    
    version = *(uint32_t*)data_ptr;
    data_ptr += sizeof(uint32_t);

    uint32_t num_members = *(uint32_t*)data_ptr;
    data_ptr+=sizeof(uint32_t);

    while (num_members--)
    {
        OSM_ENTITY_TYPE type = *(OSM_ENTITY_TYPE*)data_ptr;
        data_ptr+=sizeof(OSM_ENTITY_TYPE);
        uint64_t ref = *(uint64_t*)data_ptr;
        data_ptr+=sizeof(uint64_t);
        const char* role = (const char*)data_ptr;
        data_ptr+=strlen(role)+1;  //including zero termination
        members.push_back( OsmRelationMember(type, ref, role));
    }
   
    RawTags rawTags(data_ptr);
    
    for ( const std::pair< const char*, const char*> &kv : rawTags)
        tags.push_back( std::make_pair(kv.first, kv.second));
}

uint64_t OsmRelation::getSerializedSize() const
{
    uint64_t size = sizeof(uint64_t) + //id
                    sizeof(uint32_t) + //version
                    sizeof(uint32_t) + //numMembers
                    // type, ref and role null-termination for each member (roles string lengths are added later)
                    (sizeof(OSM_ENTITY_TYPE)+sizeof(uint64_t) + 1) * members.size() +
                    RawTags::getSerializedSize(this->tags);
                    
    for (const OsmRelationMember& mbr : members)
        size += mbr.role.length();
        
    return size;
}

void OsmRelation::serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const
{
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped way *starts*
    fwrite( &id, sizeof(id), 1, data_file);
    fwrite( &version, sizeof(version), 1, data_file);
    
    uint32_t num_members = members.size();
    fwrite(&num_members, sizeof(num_members), 1, data_file);
    
    for (const OsmRelationMember &mbr : members)
    {
        fwrite(&mbr.type, sizeof(mbr.type), 1, data_file);
        fwrite(&mbr.ref,  sizeof(mbr.ref),  1, data_file);        
        const char* str = mbr.role.c_str();
        fwrite(str, strlen(str)+1, 1, data_file);
    }

    RawTags::serialize(tags, data_file);

    if (index_map)
    {        
        ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
        uint64_t* ptr = (uint64_t*)index_map->ptr;
        ptr[id] = offset;
    }
    
}

void OsmRelation::serializeWithIndexUpdate( ChunkedFile& dataFile, mmap_t *index_map) const
{
    MUST(this->id > 0, "Invalid non-positive relation ID found. This is unsupported.");
    Chunk chunk = dataFile.createChunk(this->getSerializedSize());
    chunk.put<uint64_t>(this->id);
    chunk.put<uint32_t>(this->version);
    chunk.put<uint32_t>(this->members.size());
    
    for (const OsmRelationMember &mbr : members)
    {
        chunk.put(mbr.type);
        chunk.put(mbr.ref);
        const char* str = mbr.role.c_str();
        chunk.put(str, strlen(str)+1);
    }    
    
    RawTags::serialize(tags, chunk);

    if (index_map)
    {        
        ensure_mmap_size( index_map, (id + 1)*sizeof(uint64_t));
        ((uint64_t*)index_map->ptr)[id] = chunk.getPositionInFile();
    }
    
}


bool OsmRelation::hasKey(string key) const
{
    for (const OsmKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OsmRelation::getValue(string key) const
{
    static const string empty="";
    for (const OsmKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

    return empty;
}

ostream& operator<<(ostream &out, const OsmRelation &relation)
{
    static const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};
    out << "Relation " << relation.id << " ";
    for (const OsmRelationMember &mbr : relation.members)
    {
        out << " (" << mbr.role << " " <<ELEMENT_NAMES[(uint8_t)mbr.type] << " " << mbr.ref << ")" ;
    }
    
    out << " "<< relation.tags;
    return out;
}

uint32_t OsmRelationMember::getDataSize() const 
{ 
    return sizeof(OSM_ENTITY_TYPE) + sizeof(uint64_t) + strlen(role.c_str()) + 1; //1 byte for NULL-termination
} 

