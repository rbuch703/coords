

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

/*
std::ostream& operator <<(std::ostream& os, const OSMVertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}*/

uint64_t getSerializedSize(const std::vector<OSMKeyValuePair> &tags)
{
    uint64_t size = sizeof(uint16_t) + //uint16_t numTags
                    sizeof(uint32_t) + //uint32_t numTagBytes
                    2 * tags.size();   //zero-termination for each key and value
    for (const OSMKeyValuePair & kv : tags)
        size += (kv.first.length() + kv.second.length());
    return size;
}


void serializeTags( const vector<OSMKeyValuePair> &tags, FILE* file)
{
    assert(tags.size() < (1<<16));
    uint16_t num_tags = tags.size();
    fwrite(&num_tags, sizeof(num_tags), 1, file);

    uint32_t numTagBytes = 0;
    for (const OSMKeyValuePair &tag : tags)
    {
        numTagBytes += strlen(tag.first.c_str()) + 1;
        numTagBytes += strlen(tag.second.c_str()) + 1;
    }
    //storing this total size is redundant, but makes reading the tags back from file much faster 
    fwrite(&numTagBytes, sizeof(numTagBytes), 1, file);

    for (const OSMKeyValuePair &tag : tags)
    {
        fwrite( tag.first.c_str(),  strlen(tag.first.c_str()) + 1, 1, file);    //both including their null-termination
        fwrite( tag.second.c_str(), strlen(tag.second.c_str())+ 1, 1, file);
    }

}

void serializeTags( const vector<OSMKeyValuePair> &tags, Chunk &chunk)
{
    assert(tags.size() < (1<<16));
    chunk.put<uint16_t>(tags.size());              //numTags
    //storing this total size is redundant, but makes reading the tags back from file much faster 
    chunk.put<uint32_t>(getSerializedSize(tags)); //numTagBytes

    for (const OSMKeyValuePair &tag : tags)
    {
        //"+1" : both including their null-termination
        chunk.put( tag.first.c_str(),  strlen(tag.first.c_str())  + 1);
        chunk.put( tag.second.c_str(), strlen(tag.second.c_str()) + 1);
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

vector<OSMKeyValuePair> deserializeTags(const uint8_t* &data_ptr)
{
    vector<OSMKeyValuePair> tags;
    
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
         *       This loop is safe, because OSMKeyValuePair stores std::strings
         *       and not the raw char*, and creating the strings from the char*
         *       also creates a copy of the string data being pointed to.
        */ 
        const char* key = (const char*)data_ptr;
        data_ptr += strlen( (const char*)data_ptr)+1;
        tags.push_back( OSMKeyValuePair( key, (const char*)data_ptr));
        data_ptr += strlen( (const char*)data_ptr)+1;
    }
    
    return tags;
    
}

vector<OSMKeyValuePair> deserializeTags(FILE* src)
{
    vector<OSMKeyValuePair> tags;
    
    uint16_t num_tags;
    fread( &num_tags, sizeof(num_tags), src);
    tags.reserve(num_tags);
    
    uint32_t numTagBytes;
    fread( &numTagBytes, sizeof(numTagBytes), src);
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
         *       This loop is safe, because OSMKeyValuePair stores std::strings
         *       and not the raw char*, and creating the strings from the char*
         *       also creates a copy of the string data being pointed to.
        */ 
        const char* key = (const char*)tagBytes;
        tagBytes += strlen( (const char*)tagBytes)+1;
        assert(tagBytes < beyondTagBytes);
        
        tags.push_back( OSMKeyValuePair( key, (const char*)tagBytes));
        tagBytes += strlen( (const char*)tagBytes)+1;
        assert(tagBytes < beyondTagBytes);
    }
    
    return tags;
    
}

/*
list<OSMKeyValuePair> deserializeTags(FILE* data_file, uint64_t file_offset)
{
    fseeko(data_file, file_offset, SEEK_SET);
    return deserializeTags(data_file);
}*/

ostream& operator<<(ostream &out, const vector<OSMKeyValuePair> &tags)
{
    out << "[";
    for (vector<OSMKeyValuePair>::const_iterator it = tags.begin(); it!= tags.end(); it++)
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


OSMNode::OSMNode( const uint8_t* data_ptr)
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

uint64_t OSMNode::getSerializedSize() const
{
    return sizeof(id) + sizeof(version) + sizeof(lat) + sizeof(lon) + ::getSerializedSize(tags);
}
#if 0
OSMNode::OSMNode( FILE* idx, FILE* data, uint64_t node_id)
{
    fseek(idx, node_id*sizeof(uint64_t), SEEK_SET);
    uint64_t pos;
    int nRead = fread( &pos, sizeof(uint64_t), 1, idx);
    /*there is only a magic byte at file position 0. Pos == 0 is the marker representing that no relation 
      with that id exists */
    if ((nRead != 1) || (pos == 0))
    {
        id = -1;
        lat = INVALID_LAT_LNG;
        lon = INVALID_LAT_LNG;
        return;
    }

    fseek(data, pos, SEEK_SET);
    nRead =  fread(&lat, sizeof(int32_t), 1, data);
    nRead += fread(&lon, sizeof(int32_t), 1, data);
    id = node_id;
    if (nRead != 2)
    {
        cout << "Invalid read operation" << endl;
        exit(0);
    }

    tags = deserializeTags(data);
}
#endif

OSMNode::OSMNode( int32_t lat, int32_t lon, uint64_t  id, uint32_t version, vector<OSMKeyValuePair> tags): id(id), version(version), lat(lat), lon(lon), tags(tags) {}

void OSMNode::serializeWithIndexUpdate( FILE* dataFile, mmap_t *index_map) const
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

void OSMNode::serializeWithIndexUpdate( ChunkedFile& dataFile, mmap_t *index_map) const
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


bool OSMNode::hasKey(string key) const
{
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OSMNode::getValue(string key) const
{
    static const string empty="";
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

    return empty;
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


bool operator==(const OsmGeoPosition &a, const OsmGeoPosition &b) { return a.lat == b.lat && a.lng == b.lng; }
bool operator!=(const OsmGeoPosition &a, const OsmGeoPosition &b) { return a.lat != b.lat || a.lng != b.lng; }




OSMWay::OSMWay( uint64_t id, uint32_t version, 
            std::vector<uint64_t> way_refs, std::vector<OSMKeyValuePair> tags):
        id(id), version(version), tags(tags)  
{ 
    for (uint64_t ref : way_refs)
        refs.push_back( (OsmGeoPosition){.id = ref, .lat=INVALID_LAT_LNG, .lng = INVALID_LAT_LNG} );
}

OSMWay::OSMWay( uint64_t id, uint32_t version, 
                std::vector<OsmGeoPosition> refs, 
                std::vector<OSMKeyValuePair> tags):
                id(id), version(version), refs(refs), tags(tags) { }


OSMWay::OSMWay( const uint8_t* data_ptr)
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

//OSMWay::OSMWay(uint64_t way_id): id(way_id) {}

        

void OSMWay::serialize( FILE* data_file, mmap_t *index_map) const
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


bool OSMWay::hasKey(string key) const
{
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OSMWay::getValue(string key) const
{
    static const string empty="";
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

    return empty;
}

ostream& operator<<(ostream &out, const OSMWay &way)
{
    out << "Way " << way.id << " (";
    for ( OsmGeoPosition pos : way.refs)
        out << pos.id << ", ";

    out << ") " << way.tags;
    return out;
}



//==================================
//OsmRelation::OsmRelation( uint64_t relation_id): id(relation_id) {}

OsmRelation::OsmRelation( uint64_t id, uint32_t version, vector<OsmRelationMember> members, vector<OSMKeyValuePair> tags): id(id), version(version), members(members), tags(tags) {}

OsmRelation::OsmRelation( const uint8_t* data_ptr)
{
    id = *(uint64_t*)data_ptr;
    data_ptr += sizeof(uint64_t);
    
    version = *(uint32_t*)data_ptr;
    data_ptr += sizeof(uint32_t);

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
        members.push_back( OsmRelationMember(type, ref, role));
    }
   
    tags = deserializeTags(data_ptr);
}


void OsmRelation::initFromFile(FILE* src)
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
    
    assert(members.size() < (1<<16));
    uint32_t num_members = members.size();
    fwrite(&num_members, sizeof(num_members), 1, data_file);
    
    for (const OsmRelationMember &mbr : members)
    {
        fwrite(&mbr.type, sizeof(mbr.type), 1, data_file);
        fwrite(&mbr.ref, sizeof(mbr.ref), 1, data_file);        
        const char* str = mbr.role.c_str();
        fwrite(str, strlen(str)+1, 1, data_file);
    }
        
    serializeTags(tags, data_file);
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
    
}

bool OsmRelation::hasKey(string key) const
{
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OsmRelation::getValue(string key) const
{
    static const string empty="";
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

    return empty;
}

ostream& operator<<(ostream &out, const OsmRelation &relation)
{
    static const char* ELEMENT_NAMES[] = {"node", "way", "relation", "changeset", "other"};
    out << "Relation " << relation.id << " ";
    for (const OsmRelationMember &mbr : relation.members)
    {
        out << " (" << mbr.role << " " <<ELEMENT_NAMES[mbr.type] << " " << mbr.ref << ")" ;
    }
    
    out << " "<< relation.tags;
    return out;
}

uint32_t OsmRelationMember::getDataSize() const 
{ 
    return sizeof(ELEMENT) + sizeof(uint64_t) + strlen(role.c_str()) + 1; //1 byte for NULL-termination
} 

