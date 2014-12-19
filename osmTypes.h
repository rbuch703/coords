
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

typedef union 
{
        uint64_t id;
        struct {
            uint32_t lat, lng;
        } geo;
} OsmGeoPosition;

struct OSMWay
{
    OSMWay( uint64_t way_id);
    OSMWay( uint64_t way_id, list<uint64_t> way_refs, vector<OSMKeyValuePair> way_tags);
    OSMWay( const uint8_t* data_ptr, uint64_t way_id);

    void serialize( FILE* data_file, mmap_t *index_map) const;
    bool hasKey(string key) const;
    const string &getValue(string key) const;
    const string &operator[](string key) const {return getValue(key);}

    uint64_t id;
    list<OsmGeoPosition> refs;
    vector<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OSMWay &way);

class OsmLightweightWay {
public:
    OsmLightweightWay( FILE* src, uint64_t way_id = -1);
    OsmLightweightWay( uint8_t* data_ptr, uint64_t way_id);
    ~OsmLightweightWay ();

    OsmLightweightWay( const OsmLightweightWay &other); //disable copy constructor
    OsmLightweightWay &operator=(const OsmLightweightWay &other); // .. and assignment operator

    void serialize( FILE* data_file/*, mmap_t *index_map*/) const;

    bool     isDataMapped; //true when 'tagBytes' and 'vertices' point to areas inside a memory map
    
    OsmGeoPosition *vertices;
    uint16_t numVertices; //ways are guaranteed to have no more than 2000 nodes by the OSM specs

    uint8_t *tagBytes;
    uint32_t numTagBytes;
    uint16_t numTags;
    
    uint64_t id;
};

ostream& operator<<(ostream &out, const OsmLightweightWay &way);



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

