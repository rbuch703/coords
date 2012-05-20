
#ifndef OSM_TYPES_H
#define OSM_TYPES_H

#include <string>
#include <list>
#include <map>

#include <stdint.h> 
#include "mem_map.h"
using namespace std;

typedef enum ELEMENT { NODE, WAY, RELATION, CHANGESET, OTHER } ELEMENT;


typedef pair<string, string> OSMKeyValuePair;


struct OSMRelationMember
{
    OSMRelationMember( ELEMENT member_type, uint64_t member_ref, string member_role):
        type(member_type), ref(member_ref), role(member_role) { }

    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;
    uint32_t getDataSize() const;
    ELEMENT type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    string role;   //the role the member play to the relation
    
};

struct OSMNode
{
    OSMNode( int32_t node_lat, int32_t node_lon, uint64_t  node_id, list<OSMKeyValuePair> node_tags);
    OSMNode( FILE* data_file, uint64_t  offset, uint64_t node_id);
    OSMNode( const uint8_t* data_ptr, uint64_t node_id);
        
    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;

    int32_t lat;    //needs to be signed! -180° < lat < 180°
    int32_t lon;    //                     -90° < lon <  90°
    uint64_t id;
    list<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMNode &node);

struct OSMWay
{
    OSMWay( uint64_t way_id, list<uint64_t> way_refs, list<OSMKeyValuePair> way_tags);
    OSMWay( const uint8_t* data_ptr, uint64_t way_id);

    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;
    bool hasKey(string key) const;
    string getValue(string key) const;

    uint64_t id;
    list<uint64_t> refs;
    list<OSMKeyValuePair> tags;
    
};

ostream& operator<<(ostream &out, const OSMWay &way);

struct OSMRelation
{
    OSMRelation( uint64_t relation_id, list<OSMRelationMember> relation_members, list<OSMKeyValuePair> relation_tags);
    OSMRelation( const uint8_t* data_ptr, uint64_t relation_id);

    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;
    bool hasKey(string key) const;
    string getValue(string key) const;
    uint64_t id;
    list<OSMRelationMember> members;
    list<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMRelation &relation);

#endif
