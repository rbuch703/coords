
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
    OSMNode( uint32_t node_lat, uint32_t node_lon, uint64_t  node_id, list<OSMKeyValuePair> node_tags):
        lat(node_lat), lon(node_lon), id(node_id), tags(node_tags) {}
    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;

    uint32_t lat;
    uint32_t lon;
    uint64_t id;
    list<OSMKeyValuePair> tags;
};

struct OSMWay
{
    OSMWay( uint64_t way_id, list<uint64_t> way_refs, list<OSMKeyValuePair> way_tags) :
        id(way_id), refs(way_refs), tags(way_tags) {}

    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;
    uint64_t id;
    list<uint64_t> refs;
    list<OSMKeyValuePair> tags;
    
};

struct OSMRelation
{
    OSMRelation( uint64_t relation_id, list<OSMRelationMember> relation_members, list<OSMKeyValuePair> relation_tags):
        id(relation_id), members(relation_members), tags(relation_tags) {}
    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> &tag_symbols) const;

    uint64_t id;
    list<OSMRelationMember> members;
    list<OSMKeyValuePair> tags;
};

#endif
