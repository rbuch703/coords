
#ifndef OSM_TYPES_H
#define OSM_TYPES_H

#include <stdint.h> 
#include <assert.h>

#include <string>
#include <map>
#include <vector>

#include "mem_map.h"
#include "containers/chunkedFile.h"


enum ELEMENT : uint8_t { NODE, WAY, RELATION, CHANGESET, OTHER };
typedef std::pair<std::string, std::string> OSMKeyValuePair;

/*
struct OSMVertex
{
    OSMVertex() {}
    OSMVertex( const int32_t pX, const int32_t pY): x(pX), y(pY) {}
    
    bool operator==(OSMVertex other) { return x == other.x && y == other.y; }
    bool operator!=(OSMVertex other) { return x != other.x || y != other.y; }
public:
    int32_t x,y;
};*/
//std::ostream& operator <<(std::ostream& os, const OSMVertex v);

/** 0x7FFFFFFF is the maximum positive value in signed ints, i.e. ~ 2.1 billion
 *  In OSMs int32_t lat/lng storage, this corresponds to ~ 210°, which is outside
 *  the valid range for latitude and longitude, and thus can be used to mark
 *  invalid entries.
 *  note: 0xFFFFFFFF in two's complement is -1, and thus a valid lat/lng value.
 **/ 
const int32_t INVALID_LAT_LNG = 0x7FFFFFFF;

/* on-disk format for OsmNode:
    - uint64_t id
    - uint32_t version
    - int32_t lat
    - int32_t lon
    - <tags>
*/

struct OSMNode
{
    OSMNode( int32_t lat, int32_t lon, uint64_t  id, uint32_t version, std::vector<OSMKeyValuePair> tags = std::vector<OSMKeyValuePair>());
//    OSMNode( FILE* data_file, uint64_t  offset, uint64_t node_id);
//    OSMNode( FILE* idx, FILE* data, uint64_t node_id);
    OSMNode( const uint8_t* data_ptr);
        
    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    void serializeWithIndexUpdate( ChunkedFile &dataFile, mmap_t *index_map) const;

    const std::string &getValue(std::string key) const;
    bool hasKey(std::string key) const;
    const std::string &operator[](std::string key) const {return getValue(key);}
    
    bool operator==(const OSMNode &other) const;
    bool operator!=(const OSMNode &other) const;
    bool operator< (const OSMNode &other) const;
    uint64_t getSerializedSize() const;    
    uint64_t id;
    uint32_t version;
    int32_t lat;    //needs to be signed! -180° < lat < 180°
    int32_t lon;    //                     -90° < lon <  90°
    std::vector<OSMKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OSMNode &node);

typedef struct 
{
    uint64_t id;
    int32_t lat, lng;
} OsmGeoPosition;

bool operator==(const OsmGeoPosition &a, const OsmGeoPosition &b);
bool operator!=(const OsmGeoPosition &a, const OsmGeoPosition &b);
bool operator< (const OsmGeoPosition &a, const OsmGeoPosition &b); //just an arbitrary ordering for containers that need one

OsmGeoPosition operator-(const OsmGeoPosition &a, const OsmGeoPosition &b);

struct OSMWay
{
//    OSMWay( uint64_t way_id);
    OSMWay( uint64_t way_id, uint32_t version, 
            std::vector<uint64_t> refs = std::vector<uint64_t>(), 
            std::vector<OSMKeyValuePair> tags = std::vector<OSMKeyValuePair>());

    OSMWay( uint64_t way_id, uint32_t version, 
            std::vector<OsmGeoPosition> refs, 
            std::vector<OSMKeyValuePair> tags = std::vector<OSMKeyValuePair>());
            
    OSMWay( const uint8_t* data_ptr);

    uint64_t getSerializedSize() const;
    void serialize( FILE* data_file, mmap_t *index_map) const;
    void serialize( ChunkedFile& dataFile, mmap_t *index_map) const;
    bool hasKey(std::string key) const;
    const std::string &getValue(std::string key) const;
    const std::string &operator[](std::string key) const {return getValue(key);}

    uint64_t id;
    uint32_t version;
    std::vector<OsmGeoPosition> refs;
    std::vector<OSMKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OSMWay &way);

struct OsmRelationMember
{
    OsmRelationMember( ELEMENT member_type, uint64_t member_ref, std::string member_role):
        type(member_type), ref(member_ref), role(member_role) { }

    void serialize( FILE* data_file, mmap_t *index_map, const std::map<OSMKeyValuePair, uint8_t> *tag_symbols) const;
    uint32_t getDataSize() const;
    ELEMENT type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    std::string role;   //the role the member has as part of the relation
    
};


struct OsmRelation
{
    //OsmRelation( uint64_t relation_id);
    OsmRelation( uint64_t id, uint32_t version, 
                std::vector<OsmRelationMember> members = std::vector<OsmRelationMember>(), 
                std::vector<OSMKeyValuePair> tags = std::vector<OSMKeyValuePair>());
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
    std::vector<OSMKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmRelation &relation);
std::ostream& operator<<(std::ostream &out, const std::vector<OSMKeyValuePair> &tags);



#endif

