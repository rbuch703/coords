
#ifndef OSM_TYPES_H
#define OSM_TYPES_H

#include <string>
#include <list> //FIXME: to be removed after all lists are replaced by vectors
#include <map>
#include <vector>

#include <stdint.h> 
#include "mem_map.h"

using namespace std;

typedef enum ELEMENT { NODE, WAY, RELATION, CHANGESET, OTHER } ELEMENT;
typedef pair<string, string> OSMKeyValuePair;

struct OSMVertex
{
    OSMVertex() {}
    OSMVertex( const int32_t pX, const int32_t pY): x(pX), y(pY) {}
    
    bool operator==(OSMVertex other) { return x == other.x && y == other.y; }
    bool operator!=(OSMVertex other) { return x != other.x || y != other.y; }
public:
    int32_t x,y;
};

std::ostream& operator <<(std::ostream& os, const OSMVertex v);


struct OSMNode
{
    OSMNode( int32_t node_lat, int32_t node_lon, uint64_t  node_id, vector<OSMKeyValuePair> node_tags = vector<OSMKeyValuePair>() );
    OSMNode( FILE* data_file, uint64_t  offset, uint64_t node_id);
    OSMNode( FILE* idx, FILE* data, uint64_t node_id);
    OSMNode( const uint8_t* data_ptr, uint64_t node_id);
        
    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    const string &getValue(string key) const;
    bool hasKey(string key) const;
    const string &operator[](string key) const {return getValue(key);}
    
    bool operator==(const OSMNode &other) const;
    bool operator!=(const OSMNode &other) const;
    bool operator< (const OSMNode &other) const;
    
    OSMVertex toVertex() const {return OSMVertex(lat, lon);}
    int32_t lat;    //needs to be signed! -180° < lat < 180°
    int32_t lon;    //                     -90° < lon <  90°
    uint64_t id;
    vector<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMNode &node);

struct OSMWay
{
    OSMWay( uint64_t way_id);
    OSMWay( uint64_t way_id, list<uint64_t> way_refs, vector<OSMKeyValuePair> way_tags);
    OSMWay( const uint8_t* data_ptr, uint64_t way_id);

    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    void serialize( FILE* data_file) const;
    bool hasKey(string key) const;
    const string &getValue(string key) const;
    const string &operator[](string key) const {return getValue(key);}

    uint64_t id;
    list<uint64_t> refs;
    vector<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMWay &way);

// a representation of an OSM way that includes the data of all of its nodes (as opposed to just references to them)
struct OSMIntegratedWay
{
    //manually constructs the way
    OSMIntegratedWay( uint64_t way_id, list<OSMVertex> way_vertices, vector<OSMKeyValuePair> way_tags);
    //constructs the way based on a memory pointer (e.g. from a mmap) to serialized way data    
    OSMIntegratedWay( const uint8_t* &data_ptr, uint64_t way_id);
    //constructs the way from a file handle whose current position already points to the serialized data
    OSMIntegratedWay( FILE* src, uint64_t way_id = -1);
    /*constructs the way from files: the first file contains an index of where each way is stored in the second file
      neither one has to point to the specific way that is sought */
    OSMIntegratedWay( FILE* idx, FILE* data, uint64_t way_id);

    void initFromFile(FILE* src);

    void serialize( FILE* data_file) const;
    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    bool hasKey(string key) const;
    const string &getValue(string key) const;
    const string &operator[](string key) const {return getValue(key);}
//    PolygonSegment toPolygonSegment() const;
public:
    uint64_t id;
    list<OSMVertex> vertices;
    vector<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMIntegratedWay &way);
struct OSMRelationMember
{
    OSMRelationMember( ELEMENT member_type, uint64_t member_ref, string member_role):
        type(member_type), ref(member_ref), role(member_role) { }

    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> *tag_symbols) const;
    uint32_t getDataSize() const;
    ELEMENT type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    string role;   //the role the member play to the relation
    
};


struct OSMRelation
{
    OSMRelation( uint64_t relation_id);
    OSMRelation( uint64_t relation_id, list<OSMRelationMember> relation_members, vector<OSMKeyValuePair> relation_tags);
    OSMRelation( const uint8_t* data_ptr, uint64_t relation_id);
    OSMRelation( FILE* src, uint64_t rel_id = -1);
    OSMRelation( FILE* idx, FILE* data, uint64_t relation_id);

    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    bool hasKey(string key) const;
    const string& getValue(string key) const;
    const string& operator[](string key) const {return getValue(key);}
    void initFromFile(FILE* src);
    uint64_t id;
    list<OSMRelationMember> members;
    vector<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMRelation &relation);
ostream& operator<<(ostream &out, const list<OSMKeyValuePair> &tags);

#endif

