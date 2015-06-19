
#ifndef OSM_TYPES_H
#define OSM_TYPES_H

#include <stdint.h> 

#include <string>
#include <vector>

#include "osmBaseTypes.h"
#include "misc/mem_map.h"

/* on-disk format for OsmNode:
    - v_uint id
    - v_uint version
    - int32_t lat
    - int32_t lon
    - <tags>
*/

class ChunkedFile;

struct OsmNode
{
    OsmNode( int32_t lat, int32_t lon, uint64_t  id, uint32_t version, std::vector<OsmKeyValuePair> tags = std::vector<OsmKeyValuePair>());
    OsmNode( const uint8_t* data_ptr);
    OsmNode( FILE* f);
        
    void serialize( ChunkedFile &dataFile, mmap_t *index_map, mmap_t *vertex_data) const;

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
    int32_t lng;    //                     -90° < lon <  90°
    std::vector<OsmKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmNode &node);

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
    void serializeCompressed(FILE* f);
    uint64_t getSerializedCompressedSize() const;
    bool hasKey(std::string key) const;
    const std::string &getValue(std::string key) const;
    const std::string &operator[](std::string key) const {return getValue(key);}

    uint64_t id;
    uint32_t version;
    std::vector<OsmGeoPosition> refs;
    std::vector<OsmKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmWay &way);

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
    uint64_t id;
    uint32_t version;
    std::vector<OsmRelationMember> members;
    std::vector<OsmKeyValuePair> tags;
};

std::ostream& operator<<(std::ostream &out, const OsmRelation &relation);
std::ostream& operator<<(std::ostream &out, const std::vector<OsmKeyValuePair> &tags);



#endif

