

#include <map>
#include <ostream>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> //for exit()
#include <stdint.h>

#include "osmTypes.h"
//#include "symbolic_tags.h"

#include <iostream>


using namespace std;

uint64_t getSerializedSize(const std::vector<OsmKeyValuePair> &tags)
{
    uint64_t size = sizeof(uint16_t) + //uint16_t numTags
                    sizeof(uint32_t) + //uint32_t numTagBytes
                    2 * tags.size();   //zero-termination for each key and value

    for (const OsmKeyValuePair & kv : tags)
        size += (kv.first.length() + kv.second.length());

    return size;
}


void serializeTags( const vector<OsmKeyValuePair> &tags, FILE* file)
{
    assert(tags.size() < (1<<16));
    uint16_t num_tags = tags.size();
    MUST( fwrite(&num_tags, sizeof(num_tags), 1, file) == 1, "write error");

    uint32_t numTagBytes = 0;
    for (const OsmKeyValuePair &tag : tags)
    {
        numTagBytes += strlen(tag.first.c_str()) + 1;
        numTagBytes += strlen(tag.second.c_str()) + 1;
    }
    //storing this total size is redundant, but makes reading the tags back from file much faster 
    MUST( fwrite(&numTagBytes, sizeof(numTagBytes), 1, file) == 1, "write error");

    for (const OsmKeyValuePair &tag : tags)
    {
        //both including their null-termination
        fwrite( tag.first.c_str(),  strlen(tag.first.c_str()) + 1, 1, file);
        fwrite( tag.second.c_str(), strlen(tag.second.c_str())+ 1, 1, file);
    }

}

void serializeTags( const vector<OsmKeyValuePair> &tags, Chunk &chunk)
{
    assert(tags.size() < (1<<16));
    chunk.put<uint16_t>(tags.size());              //numTags
    //storing this total size is redundant, but makes reading the tags back from file much faster 
    //numTagBytes = serialized size minus 'numTags' and 'numTagBytes' themselves
    chunk.put<uint32_t>(getSerializedSize(tags) - sizeof(uint32_t) - sizeof(uint16_t)); 

    for (const OsmKeyValuePair &tag : tags)
    {
        //"+1" : both including their null-termination
        chunk.put( tag.first.c_str(),  strlen(tag.first.c_str())  + 1);
        chunk.put( tag.second.c_str(), strlen(tag.second.c_str()) + 1);
    }
}

vector<OsmKeyValuePair> deserializeTags(const uint8_t* &data_ptr)
{
    vector<OsmKeyValuePair> tags;
    
    uint16_t num_tags  = *((const uint16_t*)data_ptr);
    data_ptr+=2;
    
    /* just skip over numTagBytes without actually reading it.
     * We don't need this value for memory mapped input. It is stored only to 
       speed up reading back tags from files using the FILE* API*/
    //uint32_t numTagBytes = *((const uint32_t*)data_ptr);
    data_ptr +=4; 
    if (num_tags == 0) {return tags; }
    
    tags.reserve(num_tags);
    //const char* str = (const char*)data_ptr;
    while (num_tags--)
    {
        /* NOTE: using the pointers into pata_ptr as string/char* would not be
         *       safe beyond the scopof this method: the data pointer is not
         *       guaranteed to stay valid. It may be deallocated, munmap'ed or
         *       mremap'ed, invalidating the strings/char* along with it.
         *       This loop is safe, because OsmKeyValuePair stores std::strings
         *       and not the raw char*, and creating the strings from the char*
         *       also creates a copy of the string data being pointed to.
        */ 
        const char* key = (const char*)data_ptr;
        data_ptr += strlen( (const char*)data_ptr)+1;
        tags.push_back( OsmKeyValuePair( key, (const char*)data_ptr));
        data_ptr += strlen( (const char*)data_ptr)+1;
    }
    
    return tags;
    
}

vector<OsmKeyValuePair> deserializeTags(FILE* src)
{
    vector<OsmKeyValuePair> tags;
    
    uint16_t num_tags;
    MUST( fread( &num_tags, sizeof(num_tags), 1, src) == 1, "read error");
    tags.reserve(num_tags);
    
    uint32_t numTagBytes;
    MUST( fread( &numTagBytes, sizeof(numTagBytes), 1, src) == 1, "read error");
    char* tagBytes = new char[numTagBytes];
#ifndef NDEBUG 
    char* beyondTagBytes = tagBytes + numTagBytes;
#endif

    while (num_tags--)
    {
        /* NOTE: using the pointers into pata_ptr as string/char* would not be
         *       safe beyond the scopof this method: the data pointer is not
         *       guaranteed to stay valid. It may be deallocated, munmap'ed or
         *       mremap'ed, invalidating the strings/char* along with it.
         *       This loop is safe, because OsmKeyValuePair stores std::strings
         *       and not the raw char*, and creating the strings from the char*
         *       also creates a copy of the string data being pointed to.
        */ 
        const char* key = (const char*)tagBytes;
        tagBytes += strlen( (const char*)tagBytes)+1;
        assert(tagBytes < beyondTagBytes);
        
        tags.push_back( OsmKeyValuePair( key, (const char*)tagBytes));
        tagBytes += strlen( (const char*)tagBytes)+1;
        assert(tagBytes < beyondTagBytes);
    }
    
    return tags;
    
}


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

    tags = deserializeTags(data_ptr);
}

uint64_t OsmNode::getSerializedSize() const
{
    return sizeof(id) + sizeof(version) + sizeof(lat) + sizeof(lon) + ::getSerializedSize(tags);
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

    serializeTags( tags, dataFile );

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

    serializeTags( tags, chunk );

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


bool operator==(const OsmGeoPosition &a, const OsmGeoPosition &b) { return a.lat == b.lat && a.lng == b.lng; }
bool operator!=(const OsmGeoPosition &a, const OsmGeoPosition &b) { return a.lat != b.lat || a.lng != b.lng; }
bool operator< (const OsmGeoPosition &a, const OsmGeoPosition &b) { return a.lat < b.lat || (a.lat == b.lat && a.lng < b.lng);}



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
    tags = deserializeTags(data_ptr);
}

uint64_t OsmWay::getSerializedSize() const
{
    return sizeof(uint64_t) + //id
           sizeof(uint32_t) + //version
           sizeof(uint16_t) + // numRefs
           sizeof(OsmGeoPosition) * refs.size() + //node refs
           ::getSerializedSize(tags);
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
    
    serializeTags(tags, data_file);

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

    serializeTags(this->tags, chunk);
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
//OsmRelation::OsmRelation( uint64_t relation_id): id(relation_id) {}

OsmRelation::OsmRelation( uint64_t id, uint32_t version, vector<OsmRelationMember> members, vector<OsmKeyValuePair> tags): id(id), version(version), members(members), tags(tags) {}

OsmRelation::OsmRelation( const uint8_t* data_ptr)
{
    id = *(uint64_t*)data_ptr;
    data_ptr += sizeof(uint64_t);
    
    version = *(uint32_t*)data_ptr;
    data_ptr += sizeof(uint32_t);

    uint32_t num_members = *(uint32_t*)data_ptr;
    data_ptr+=sizeof(uint32_t);

    //uint32_t members_data_size = *(uint32_t)data_ptr; //don't need these, but their are still present in the data file
    //data_ptr+=4;
    
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
   
    tags = deserializeTags(data_ptr);
}

uint64_t OsmRelation::getSerializedSize() const
{
    uint64_t size = sizeof(uint64_t) + //id
                    sizeof(uint32_t) + //version
                    sizeof(uint32_t) + //numMembers
                    // type, ref and role null-termination for each member (roles string lengths are added later)
                    (sizeof(OSM_ENTITY_TYPE)+sizeof(uint64_t) + 1) * members.size() +
                    ::getSerializedSize(this->tags);
                    
    for (const OsmRelationMember& mbr : members)
        size += mbr.role.length();
        
    return size;
}


void OsmRelation::initFromFile(FILE* src)
{
    uint32_t num_members;
    if (1 != fread(&num_members, sizeof(num_members), 1, src)) return;
    
    while (num_members--)
    {
        OSM_ENTITY_TYPE type;
        if (1 != fread (&type, sizeof(OSM_ENTITY_TYPE), 1, src)) return;
        
        uint64_t ref;
        if (1 != fread (&ref, sizeof(ref), 1, src)) return;
        
        int i;
        string s;
        while ( (i = fgetc(src)) != EOF  && i != '\0')
            s+=i;
        //data_ptr+=strlen(role)+1;  //including zero termination
        members.push_back( OsmRelationMember(type, ref, s));
    }
   
    tags = deserializeTags(src);

}

#if 0
OsmRelation::OsmRelation( FILE* src, uint64_t rel_id): id(rel_id)
{
    this->initFromFile(src);
}


OsmRelation::OsmRelation( FILE* idx, FILE* data, uint64_t relation_id): id(relation_id)
{
    fseek(idx, relation_id*sizeof(uint64_t), SEEK_SET);
    uint64_t pos;
    int nRead = fread( &pos, sizeof(uint64_t), 1, idx);
    /*there is only a magic byte at file position 0. Pos == 0 is the marker representing that no relation 
      with that id exists */
    if ((nRead != 1) || (pos == 0))
    {
        id = -1;
        return;
    }

    fseek(data, pos, SEEK_SET);
    this->initFromFile(data);
}
#endif

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

    serializeTags(tags, data_file);

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
    
    serializeTags(tags, chunk);

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

