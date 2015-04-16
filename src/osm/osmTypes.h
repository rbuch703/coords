
#ifndef OSM_TYPES_H
#define OSM_TYPES_H

#include <stdint.h> 
#include <assert.h>

#include <string>
#include <map>
#include <vector>

#include "mem_map.h"
#include "containers/chunkedFile.h"


enum struct OSM_ENTITY_TYPE : uint8_t { NODE, WAY, RELATION, CHANGESET, OTHER };

typedef std::pair<std::string, std::string> OsmKeyValuePair;


/* on-disk format for OsmNode:
    - uint64_t id
    - uint32_t version
    - int32_t lat
    - int32_t lon
    - <tags>
*/
struct OsmNode
{
    OsmNode( int32_t lat, int32_t lon, uint64_t  id, uint32_t version, std::vector<OsmKeyValuePair> tags = std::vector<OsmKeyValuePair>());
    OsmNode( const uint8_t* data_ptr);
        
    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    void serializeWithIndexUpdate( ChunkedFile &dataFile, mmap_t *index_map) const;

    const std::string &getValue(std::string key) const;
    bool hasKey(std::string key) const;
    const std::string &operator[](std::string key) const {return getValue(key);}
    
    bool operator==(const OsmNode &other) const;
    bool operator!=(const OsmNode &other) const;
    bool operator< (const OsmNode &other) const;
    uint64_t getSerializedSize() const;    
    uint64_t id;
    uint32_t version;
    int32_t lat;    //needs to be signed! -180° < lat < 180°
    int32_t lon;    //                     -90° < lon <  90°
    std::vector<OsmKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmNode &node);

typedef struct 
{
    uint64_t id;
    int32_t lat, lng;
} OsmGeoPosition;

bool operator==(const OsmGeoPosition &a, const OsmGeoPosition &b);
bool operator!=(const OsmGeoPosition &a, const OsmGeoPosition &b);
bool operator< (const OsmGeoPosition &a, const OsmGeoPosition &b); //just an arbitrary ordering for containers that need one

OsmGeoPosition operator-(const OsmGeoPosition &a, const OsmGeoPosition &b);

struct OsmWay
{
//    OSMWay( uint64_t way_id);
    OsmWay( uint64_t way_id, uint32_t version, 
            std::vector<uint64_t> refs = std::vector<uint64_t>(), 
            std::vector<OsmKeyValuePair> tags = std::vector<OsmKeyValuePair>());

    OsmWay( uint64_t way_id, uint32_t version, 
            std::vector<OsmGeoPosition> refs, 
            std::vector<OsmKeyValuePair> tags = std::vector<OsmKeyValuePair>());
            
    OsmWay( const uint8_t* data_ptr);

    uint64_t getSerializedSize() const;
    void serialize( FILE* data_file, mmap_t *index_map) const;
    void serialize( ChunkedFile& dataFile, mmap_t *index_map) const;
    bool hasKey(std::string key) const;
    const std::string &getValue(std::string key) const;
    const std::string &operator[](std::string key) const {return getValue(key);}

    uint64_t id;
    uint32_t version;
    std::vector<OsmGeoPosition> refs;
    std::vector<OsmKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmWay &way);

struct OsmRelationMember
{
    OsmRelationMember( OSM_ENTITY_TYPE member_type, uint64_t member_ref, std::string member_role):
        type(member_type), ref(member_ref), role(member_role) { }

    uint32_t getDataSize() const;
    OSM_ENTITY_TYPE type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    std::string role;   //the role the member has as part of the relation
    
};


struct OsmRelation
{
    //OsmRelation( uint64_t relation_id);
    OsmRelation( uint64_t id, uint32_t version, 
                std::vector<OsmRelationMember> members = std::vector<OsmRelationMember>(), 
                std::vector<OsmKeyValuePair> tags = std::vector<OsmKeyValuePair>());
    OsmRelation( const uint8_t* data_ptr);
/*    OsmRelation( FILE* src, uint64_t rel_id = -1);
    OsmRelation( FILE* idx, FILE* data, uint64_t relation_id);*/

    uint64_t getSerializedSize() const;

    void serializeWithIndexUpdate( FILE* dataFile, mmap_t *index_map) const;
    void serializeWithIndexUpdate( ChunkedFile& dataFile, mmap_t *index_map) const;
    bool hasKey(std::string key) const;
    const std::string& getValue(std::string key) const;
    const std::string& operator[](std::string key) const {return getValue(key);}
    void initFromFile(FILE* src);
    uint64_t id;
    uint32_t version;
    std::vector<OsmRelationMember> members;
    std::vector<OsmKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmRelation &relation);
std::ostream& operator<<(std::ostream &out, const std::vector<OsmKeyValuePair> &tags);



#endif

