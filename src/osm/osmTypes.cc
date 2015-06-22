

#include <map>
#include <ostream>

#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> //for exit()
#include <stdint.h>

#include "config.h"
#include "osmTypes.h"
#include "misc/rawTags.h"
#include "misc/varInt.h"
#include "containers/chunkedFile.h"
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
    int nRead = 0;
    
    id = varUintFromBytes(data_ptr, &nRead);
    data_ptr += nRead;

    version = varUintFromBytes(data_ptr, &nRead);
    data_ptr+= nRead;
    
    lat = *(int32_t*)data_ptr;
    data_ptr+=4;

    lng = *(int32_t*)data_ptr;
    data_ptr+=4;

    for (std::pair<const char*, const char*> kv : RawTags(data_ptr))
        tags.push_back( std::make_pair(kv.first, kv.second));
}

OsmNode::OsmNode( FILE* f)
{
    id = varUintFromFile(f, nullptr);
    version = varUintFromFile(f, nullptr);
    MUST(fread(&lat, sizeof(lat), 1, f) == 1, "node read error");
    MUST(fread(&lng, sizeof(lng), 1, f) == 1, "node read error");
    
    //size_t pos = ftell(f);
    uint64_t numTagBytes = varUintFromFile(f, nullptr);
    
    uint64_t numBytes = numTagBytes + varUintNumBytes(numTagBytes);
    uint8_t *bytes = new uint8_t[numBytes];
    
    int nRead = varUintToBytes( numTagBytes, bytes);
    MUST(fread( bytes+nRead, numTagBytes, 1, f) == 1, "node read error");
    
    const uint8_t *tmp = bytes;
    for (std::pair<const char*, const char*> kv : RawTags(tmp))
        tags.push_back( std::make_pair(kv.first, kv.second));
    
    delete [] bytes;
    
}


uint64_t OsmNode::getSerializedSize() const
{
    uint64_t tagsSize = RawTags::getSerializedSize(tags);
    return varUintNumBytes(id) + 
           varUintNumBytes(version) + 
           sizeof(lat) + 
           sizeof(lng) + 
           varUintNumBytes(tagsSize) +  //size of 'tags size' field
           tagsSize;                    //size of actual tags
}

OsmNode::OsmNode( int32_t lat, int32_t lng, uint64_t  id, uint32_t version, vector<OsmKeyValuePair> tags): id(id), version(version), lat(lat), lng(lng), tags(tags) {}

void OsmNode::serialize( ChunkedFile &dataFile, mmap_t *index_map, mmap_t *vertex_data) const
{
    /** temporary nodes in OSM editors are allowed to have negative node IDs, 
      * but those in the official maps are guaranteed to be positive.
      * Also, negative ids would mess up the memory map (would lead to negative indices*/
    MUST (id > 0, "Invalid non-positive node id in input file.");

    //serialize lat/lng to vertex storage
    ensure_mmap_size( vertex_data, (this->id+1) * 2 * sizeof(int32_t));
    int32_t* vertex_ptr = (int32_t*)vertex_data->ptr;
    vertex_ptr[2*this->id]   = this->lat;
    vertex_ptr[2*this->id+1] = this->lng;

    ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
    uint64_t* index_ptr = (uint64_t*)index_map->ptr;

    /* if the whole node data (excluding lat/lng, which has been stored above already) would
     * fit into the index entry that normally *points* to the node data, write the data
     * into the index directly */
    /* serialization format:
     * bytes 0-6:
     * -    varUint id
     * -    varUint version
     * byte 7: 0xFF
     */
    if (this->tags.size() == 0 && (varUintNumBytes(this->id) + varUintNumBytes(this->version) +1 <= 8))
    {
        uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0xFF};
        int nRead = varUintToBytes( this->id, data);
        nRead += varUintToBytes( this->version, data+nRead);
        MUST( nRead < 8, "serialization overflow");
        memcpy( &index_ptr[this->id], data, 8);
    } else  //full serialization
    {
        Chunk chunk = dataFile.createChunk( this->getSerializedSize());

        int numBytes;
        uint8_t bytes[10];
        
        numBytes = varUintToBytes(id, bytes);
        chunk.put(bytes, numBytes);

        numBytes = varUintToBytes(version, bytes);
        chunk.put(bytes, numBytes);
        chunk.put(lat);
        chunk.put(lng);

        RawTags::serialize( tags, chunk );

        //std::cout << id << endl;    
        ensure_mmap_size( index_map, (id+1)*sizeof(uint64_t));
        index_ptr[id] = chunk.getPositionInFile();
    }

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


bool OsmNode::operator==(const OsmNode &other) const {return lat == other.lat && lng == other.lng;}
bool OsmNode::operator!=(const OsmNode &other) const {return lat != other.lat || lng != other.lng;}
bool OsmNode::operator< (const OsmNode &other) const {return id < other.id;}

ostream& operator<<(ostream &out, const OsmNode &node)
{
    out << "Node " << node.id <<" (" << (node.lat/10000000.0) << "°, " << (node.lng/10000000.0) << "°)";
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


OsmWay::OsmWay( const uint8_t* &data)
{
    
    int nRead = 0;
    this->id = varUintFromBytes(data, &nRead);
    data += nRead;
    
    this->version = varUintFromBytes(data, &nRead);
    data += nRead;
    
    uint64_t numNodes = varUintFromBytes(data, &nRead);
    data += nRead;
    
    MUST( numNodes <= 2000, "More node refs in way than specification allows");
    
    this->refs.reserve(numNodes);
    
    int64_t prevId  = 0;
    int32_t prevLat = 0;
    int32_t prevLng = 0;    
    
    
    while (numNodes--)
    {
        prevId += varIntFromBytes(data, &nRead);
        data += nRead;
        
        prevLat += varIntFromBytes(data, &nRead);
        data += nRead;

        prevLng += varIntFromBytes(data, &nRead);
        data += nRead;
    
        this->refs.push_back( (OsmGeoPosition){ .id = (uint64_t)prevId, 
                              .lat = prevLat,
                              .lng = prevLng} );
    }
    
    uint64_t nTagBytes = 0;
    RawTags rawTags(data, &nTagBytes);
    data += nTagBytes;
    
    for ( const std::pair< const char*, const char*> &kv : rawTags)
        tags.push_back( std::make_pair(kv.first, kv.second));
}

uint64_t OsmWay::getSerializedSize(uint64_t *numTagBytesOut) const
{
    uint64_t nTagBytes = RawTags::getSerializedSize(tags);
    if (numTagBytesOut)
        *numTagBytesOut = nTagBytes;    
    
    uint64_t nBytes = varUintNumBytes(this->id) + 
                      varUintNumBytes(this->version) + 
                      varUintNumBytes(this->refs.size()) +
                      nTagBytes + 
                      varUintNumBytes(nTagBytes);
                      
    int64_t prevRef = 0;
    int64_t prevLat = 0;
    int64_t prevLng = 0;
    for (OsmGeoPosition pos : this->refs)
    {
        int64_t dRef = pos.id -  prevRef;
        int64_t dLat = pos.lat - prevLat;
        int64_t dLng = pos.lng - prevLng;
        
        nBytes += varIntNumBytes(dRef);
        nBytes += varIntNumBytes(dLat);
        nBytes += varIntNumBytes(dLng);
        
        prevRef = pos.id;
        prevLat = pos.lat;
        prevLng = pos.lng;
    }

    return nBytes;
}


void OsmWay::serialize(FILE* f)
{
    uint64_t nBytes = 0;
    uint8_t * bytes = this->serialize(&nBytes);
    
    MUST( fwrite( bytes, nBytes, 1, f) == 1, "compresses way write error");
    delete [] bytes;
}

void OsmWay::serialize(uint8_t* dest, uint64_t serializedSize, uint64_t nTagBytes)
{
    uint8_t* pos = dest;
    
    pos += varUintToBytes( this->id, pos);
    pos += varUintToBytes( this->version, pos);
    pos += varUintToBytes( this->refs.size(), pos);
    
    int64_t prevRef = 0;
    int64_t prevLat = 0;
    int64_t prevLng = 0;
    for (OsmGeoPosition p : this->refs)
    {
        int64_t dRef = p.id -  prevRef;
        int64_t dLat = p.lat - prevLat;
        int64_t dLng = p.lng - prevLng;
        
        pos += varIntToBytes(dRef, pos);
        pos += varIntToBytes(dLat, pos);
        pos += varIntToBytes(dLng, pos);
        
        prevRef = p.id;
        prevLat = p.lat;
        prevLng = p.lng;
    }
    
    RawTags::serialize(this->tags, nTagBytes, pos, nTagBytes + varUintNumBytes(nTagBytes));
    pos += nTagBytes + varUintNumBytes(nTagBytes);
    //std::cout << (pos - bytes) << ", " << nBytes << std::endl;
    MUST( pos - dest == (int64_t)serializedSize, "compressed way serialization size mismatch");

}

uint8_t* OsmWay::serialize(uint64_t *numBytes)
{
    uint64_t nTagBytes = RawTags::getSerializedSize(this->tags);
    uint64_t nBytes = this->getSerializedSize();
    uint8_t *bytes = new uint8_t[nBytes];
    
    this->serialize(bytes, nBytes, nTagBytes);

    if (numBytes)    
        *numBytes = nBytes;
        
    return bytes;
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

static uint64_t tryParse(const char* s, uint64_t defaultValue)
{
    uint64_t res = 0;
    for ( ; *s != '\0'; s++)
    {
        if ( *s < '0' || *s > '9') return defaultValue;
        res = (res * 10) + ( *s - '0');
        
    }
    return res;
}

/* addBoundaryTags() adds tags from boundary relations that refer to a way to said way.
   Since a way may be part of multiple boundary relations (e.g. country boundaries are usually
   also the boundaries of a state inside that country), care must be taken when multiple referring
   relations are about to add the same tags to a way.
   
   Algorithm: the admin_level of the way is determined (if it has one itself, otherwise
              the dummy value 0xFFFF is used). Afterwards,
              the tags from all boundary relations are added as follows: when a relation
              has a numerically smaller (or equal) admin level to that of the way
              (i.e. it is a more important boundary), all of its tags are added to the way
              (even if that means overwriting existing tags) and the way's admin level is
              updated. If the relation has a less important admin level, only those of its
              tags are added to the way that do not overwrite existing tags.

  */

static void addBoundaryTags( TagDictionary &tags, 
                             const std::vector<uint64_t> &referringRelations, 
                             const std::map<uint64_t, TagDictionary> &boundaryTags)
{

    uint64_t adminLevel = tags.count("admin_level") ? tryParse(tags["admin_level"].c_str(), 0xFFFF) : 0xFFFF;
    //cout << "admin level is " << adminLevel << endl;

    for ( uint64_t relId : referringRelations)
    {
        if (!boundaryTags.count(relId))
            continue;
            
        const TagDictionary &relationTags = boundaryTags.at(relId);
        uint64_t relationAdminLevel = relationTags.count("admin_level") ?
                                      tryParse(relationTags.at("admin_level").c_str(), 0xFFFF) :
                                      0xFFFF;
        
        for ( const OsmKeyValuePair &kv : relationTags)
        {
            /* is only the relation type marking the relation as a boundary. This was already
             * processed when creating the boundaryTags map, and need not be considered further*/
            if (kv.first == "type")
                continue;
                
            if ( ! tags.count(kv.first))    //add if not already exists
                tags.insert( kv);
            else if (relationAdminLevel <= adminLevel)  //overwrite only if from a more important boundary
                tags[kv.first] = kv.second;
            
        }

        if (relationAdminLevel < adminLevel)
        {
            adminLevel = relationAdminLevel;
            //cout << "\tadmin level is now " << adminLevel << endl;
            //assert(adminLevel != 1024);
        }
    }
}


void OsmWay::addTagsFromBoundaryRelations(
    std::vector<uint64_t> referringRelationIds,
    const std::map<uint64_t, TagDictionary> &boundaryRelationTags)
{
    uint64_t i = 0;
    while (i < referringRelationIds.size())
    {
        if (! boundaryRelationTags.count(referringRelationIds[i]))
        {
            referringRelationIds[i] = referringRelationIds.back();
            referringRelationIds.pop_back();
        } else
            i++;
    }
    
    if (referringRelationIds.size() == 0)
        return;

    TagDictionary tagDict(this->tags.begin(), this->tags.end());
    addBoundaryTags( tagDict, referringRelationIds, boundaryRelationTags );

    this->tags = Tags(tagDict.begin(), tagDict.end());
}

bool OsmWay::isClosed() const
{
    return (refs.size() > 3) && 
           (refs.front().lat == refs.back().lat) &&
           (refs.front().lng == refs.back().lng);
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
    uint64_t tagsSize = RawTags::getSerializedSize(this->tags);
    uint64_t size = sizeof(uint64_t) + //id
                    sizeof(uint32_t) + //version
                    sizeof(uint32_t) + //numMembers
                    // type, ref and role null-termination for each member (roles string lengths are added later)
                    (sizeof(OSM_ENTITY_TYPE)+sizeof(uint64_t) + 1) * members.size() +
                    varUintNumBytes(tagsSize) +
                    tagsSize;
                    
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

