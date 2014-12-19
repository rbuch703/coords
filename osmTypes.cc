

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

#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}


std::ostream& operator <<(std::ostream& os, const OSMVertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}


/** on-file layout for tags :
    uint16_t num_verbose_tags;
    uint32_t verbose_tag_data_len; (total data size of all verbose tags, in bytes); optional, present only if num_verbose_tags > 0
    <2*num_verbose_tags> zero-terminated strings containing interleaved keys and values 
    (key_0, value_0, key_1, value_1, ...)

*/
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

/*
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

}*/

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

OSMNode::OSMNode( const uint8_t* data_ptr, uint64_t node_id)
{
    lat = *(int32_t*)data_ptr;
    data_ptr+=4;
    lon = *(int32_t*)data_ptr;
    data_ptr+=4;
    tags = deserializeTags(data_ptr);
    id = node_id;
}

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
        lat = 0;
        lon = 0;
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


OSMNode::OSMNode( int32_t node_lat, int32_t node_lon, uint64_t  node_id, vector<OSMKeyValuePair> node_tags):
        lat(node_lat), lon(node_lon), id(node_id), tags(node_tags) {}

void OSMNode::serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    assert (id > 0);  

    uint64_t offset = ftello(data_file);    //get offset at which the dumped node *starts*
    fwrite( &lat, sizeof(lat), 1, data_file);
    fwrite( &lon, sizeof(lon), 1, data_file);

    serializeTags( tags, data_file );

    //std::cout << id << endl;    
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
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

OSMWay::OSMWay( uint64_t way_id, list<uint64_t> way_refs, vector<OSMKeyValuePair> way_tags):
        id(way_id), tags(way_tags)  
{ 
    for (uint64_t ref : way_refs)
        refs.push_back( (OsmGeoPosition){.id = ref} );
}

OSMWay::OSMWay( const uint8_t* data_ptr, uint64_t way_id): id(way_id)
{
    uint16_t num_node_refs = *(uint16_t*)data_ptr;
    data_ptr+= sizeof(uint16_t);
    while (num_node_refs--)
    {
        refs.push_back( (OsmGeoPosition){.id = *(uint64_t*)data_ptr} );
        data_ptr+=8;
    }
    tags = deserializeTags(data_ptr);
}

OSMWay::OSMWay(uint64_t way_id): id(way_id) {}

        

void OSMWay::serialize( FILE* data_file, mmap_t *index_map) const
{
    assert (id > 0);  
    //get offset at which the dumped way *starts*
    uint64_t offset = index_map ? ftello(data_file) : 0;
    
    MUST(refs.size() <= 2000, "#refs in way beyond what's allowed by spec");
    uint16_t num_node_refs = refs.size();
    fwrite(&num_node_refs, sizeof(num_node_refs), 1, data_file);
    
    for (OsmGeoPosition ref : refs)
        fwrite(&ref, sizeof(ref), 1, data_file);
    
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
OsmLightweightWay::OsmLightweightWay( FILE* src, uint64_t way_id): 
        isDataMapped(false), vertices(NULL), tagBytes(NULL), id(way_id)
{
    MUST( 1 == fread(&this->numVertices, sizeof(uint16_t), 1, src), "read failure");
        
    if (this->numVertices)
    {
        this->vertices = new OsmGeoPosition[this->numVertices];
        MUST( 1 == fread(this->vertices, sizeof(OsmGeoPosition) * this->numVertices, 1, src), "read failure");
    }
    
    //lightweight ways don't need the numTags themselves, but they are required to re-serialize the way 
    MUST( 1 == fread(&this->numTags,    sizeof(uint16_t), 1, src), "read failure");
    MUST( 1 == fread(&this->numTagBytes, sizeof(uint32_t), 1, src), "read failure");
    assert(this->numTagBytes <= 1<<17);
    
    if (this->numTagBytes)
    {
        this->tagBytes = new uint8_t[this->numTagBytes];
        MUST( 1 == fread(this->tagBytes, sizeof(uint8_t) * this->numTagBytes, 1, src), "read failure");
    }
}

OsmLightweightWay::OsmLightweightWay( uint8_t *dataPtr, uint64_t way_id): 
    isDataMapped(true), id(way_id)
{
    this->numVertices = *( (uint16_t*)(dataPtr));
    this->vertices = (OsmGeoPosition*)(dataPtr + 2);
    dataPtr += (2 + sizeof(OsmGeoPosition) * this->numVertices);

    this->numTags = *((uint16_t*)dataPtr);
    this->numTagBytes  = *((uint32_t*) (dataPtr + 2));
    this->tagBytes = (dataPtr + 6); 
}


OsmLightweightWay::~OsmLightweightWay()
{
    if (!isDataMapped)
    {
        delete [] vertices;
        delete [] tagBytes;
    }
}

void OsmLightweightWay::serialize( FILE* dest/*, mmap_t *index_map*/) const
{
    assert (id > 0);  
    //get offset at which the dumped way *starts*
    //uint64_t offset = index_map ? ftello(dest) : 0;
    
    MUST(this->numVertices <= 2000, "#refs in way beyond what's allowed by spec");
    MUST(1 == fwrite(&this->numVertices, sizeof(this->numVertices), 1, dest), "write error");

    if(this->numVertices > 0)
        MUST(1 == fwrite( this->vertices, sizeof(OsmGeoPosition) * this->numVertices, 1, dest), "write error");

    assert(this->numTagBytes <= 1<<17);
    MUST( 1 == fwrite(&this->numTags,     sizeof(uint16_t), 1, dest), "write error");
    MUST( 1 == fwrite(&this->numTagBytes, sizeof(uint32_t), 1, dest), "write error");
    
    if (this->numTagBytes > 0)
        MUST( 1 == fwrite(this->tagBytes, sizeof(uint8_t) * this->numTagBytes, 1, dest), "read failure");
}

ostream& operator<<(ostream &out, const OsmLightweightWay &way)
{
    out << "Way " << way.id << " (";
    for ( int i = 0; i < way.numVertices; i++)
        out << way.vertices[i].id << ", ";

    out << ") - " << way.numTagBytes << " tag bytes";
    return out;

}


//==================================
OSMRelation::OSMRelation( uint64_t relation_id): id(relation_id) {}

OSMRelation::OSMRelation( uint64_t relation_id, list<OSMRelationMember> relation_members, vector<OSMKeyValuePair> relation_tags):
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


void OSMRelation::initFromFile(FILE* src)
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

OSMRelation::OSMRelation( FILE* src, uint64_t rel_id): id(rel_id)
{
    this->initFromFile(src);
}


OSMRelation::OSMRelation( FILE* idx, FILE* data, uint64_t relation_id): id(relation_id)
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


void OSMRelation::serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const
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

    serializeTags(tags, data_file);
    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* ptr = (uint64_t*)index_map->ptr;
    ptr[id] = offset;
    
}

bool OSMRelation::hasKey(string key) const
{
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return true;

    return false;
}

const string& OSMRelation::getValue(string key) const
{
    static const string empty="";
    for (const OSMKeyValuePair &kv : tags)
        if (kv.first == key) return kv.second;

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

