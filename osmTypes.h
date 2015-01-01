
#ifndef OSM_TYPES_H
#define OSM_TYPES_H

#include <string>
#include <list> //FIXME: to be removed after all lists are replaced by vectors
#include <map>
#include <vector>

#include <stdint.h> 
#include "mem_map.h"

#include <assert.h>

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

/** 0x7FFFFFFF is the maximum positive value in signed ints, i.e. ~ 2.1 billion
 *  In OSMs int32_t lat/lng storage, this corresponds to ~ 210°, which is outside
 *  the valid range for latitude and longitude, and this can be used to mark
 *  invalid entries.
 *  note: 0xFFFFFFFF in two's complement is -1, and thus a valid lat/lng value.
 **/ 
const int32_t INVALID_LAT_LNG = 0x7FFFFFFF;

typedef struct 
{
    uint64_t id;
    int32_t lat, lng;
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
    
    /** true when 'tagBytes' and 'vertices' point to areas inside a memory map,
        and thus any changes to 'tagBytes' and 'vertices' will directly change
        the data in the underlying file */
    bool     isDataMapped; 
    
    OsmGeoPosition *vertices;
    uint16_t numVertices; //ways are guaranteed to have no more than 2000 nodes by the OSM specs

    uint8_t *tagBytes;
    uint32_t numTagBytes;
    uint16_t numTags;
    
    uint64_t id;
};

ostream& operator<<(ostream &out, const OsmLightweightWay &way);


class OsmLightweightWayStore {
public:
    OsmLightweightWayStore(const char* indexFileName, const char* dataFileName) {
        mapWayIndex = init_mmap(indexFileName, true, false);
        mapWayData  = init_mmap(dataFileName, true, true);

    }

    OsmLightweightWay operator[](uint64_t wayId)
    {
        uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
        assert(wayIndex[wayId] != 0 && "trying to access non-existent way");
        uint64_t wayPos = wayIndex[wayId];
        return OsmLightweightWay((uint8_t*)mapWayData.ptr + wayPos, wayId);

    }
    
    bool exists(uint64_t wayId) 
    {
        uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
        if (wayId >= getMaxNumWays())
            return false;
            
        return wayIndex[wayId] != 0;
    }
private:
    class LightweightWayIterator;
public:
    
    LightweightWayIterator begin() { return LightweightWayIterator(*this, 0);}
    LightweightWayIterator end()   { return LightweightWayIterator(*this, getMaxNumWays());}

    /** \returns:
     *       an upper bound for the number of ways, which is also an upper bound for the
     *       highest wayId present in the data.  
    */
    uint64_t getMaxNumWays() const { return mapWayIndex.size / sizeof(uint64_t); }

private:
    class LightweightWayIterator {
    public:
        LightweightWayIterator( OsmLightweightWayStore &host, uint64_t pos): host(host), pos(pos) 
        {
            advanceToNextWay();
        }
        
        LightweightWayIterator& operator++() {
            pos++;
            advanceToNextWay();
            return *this;
        }

        void advanceToNextWay() {
            uint64_t endPos = host.getMaxNumWays();
            uint64_t *wayIndex = (uint64_t*)host.mapWayIndex.ptr;
            while (wayIndex[pos] == 0 && pos < endPos)
                pos+=1;            
        }        

        OsmLightweightWay operator *() {
            return host[pos];
        }
        
        bool operator !=( LightweightWayIterator &other) { return pos != other.pos;} 
    
    private:
        OsmLightweightWayStore &host;
        uint64_t pos;
    };

    mmap_t mapWayIndex;
    mmap_t mapWayData;


};




struct OsmRelationMember
{
    OsmRelationMember( ELEMENT member_type, uint64_t member_ref, string member_role):
        type(member_type), ref(member_ref), role(member_role) { }

    void serialize( FILE* data_file, mmap_t *index_map, const map<OSMKeyValuePair, uint8_t> *tag_symbols) const;
    uint32_t getDataSize() const;
    ELEMENT type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    string role;   //the role the member play to the relation
    
};


struct OsmRelation
{
    OsmRelation( uint64_t relation_id);
    OsmRelation( uint64_t relation_id, list<OsmRelationMember> relation_members, vector<OSMKeyValuePair> relation_tags);
    OsmRelation( const uint8_t* data_ptr, uint64_t relation_id);
    OsmRelation( FILE* src, uint64_t rel_id = -1);
    OsmRelation( FILE* idx, FILE* data, uint64_t relation_id);

    void serializeWithIndexUpdate( FILE* data_file, mmap_t *index_map) const;
    bool hasKey(string key) const;
    const string& getValue(string key) const;
    const string& operator[](string key) const {return getValue(key);}
    void initFromFile(FILE* src);
    uint64_t id;
    list<OsmRelationMember> members;
    vector<OSMKeyValuePair> tags;
};

ostream& operator<<(ostream &out, const OsmRelation &relation);
ostream& operator<<(ostream &out, const list<OSMKeyValuePair> &tags);

class RelationStore {
public:
    RelationStore(const char* indexFileName, const char* dataFileName) {
        mapRelationIndex = init_mmap(indexFileName, true, false);
        mapRelationData  = init_mmap(dataFileName, true, true);
    }

    OsmRelation operator[](uint64_t relationId)
    {
        uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
        assert(relationIndex[relationId] != 0 && "trying to access non-existent relation");
        uint64_t dataOffset = relationIndex[relationId];
        return OsmRelation((uint8_t*)mapRelationData.ptr + dataOffset, relationId);

    }
    
    bool exists(uint64_t relationId) 
    {
        uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
        if (relationId >= getMaxNumRelations())
            return false;
            
        return relationIndex[relationId] != 0;
    }
private:
    class RelationIterator;
public:
    
    RelationIterator begin() { return RelationIterator(*this, 0);}
    RelationIterator end()   { return RelationIterator(*this, getMaxNumRelations());}

    uint64_t getMaxNumRelations() const { return mapRelationIndex.size / sizeof(uint64_t); }

private:
    class RelationIterator {
    public:
        RelationIterator( RelationStore &host, uint64_t pos): host(host), pos(pos) 
        {
            advanceToNextRelation();
        }
        
        RelationIterator& operator++() {
            pos++;
            advanceToNextRelation();
            return *this;
        }

        void advanceToNextRelation() {
            uint64_t endPos = host.getMaxNumRelations();
            uint64_t *relationIndex = (uint64_t*)host.mapRelationIndex.ptr;
            while (relationIndex[pos] == 0 && pos < endPos)
                pos+=1;            
        }        

        OsmRelation operator *() {
            return host[pos];
        }
        
        bool operator !=( RelationIterator &other) { return pos != other.pos;} 
    
    private:
        RelationStore &host;
        uint64_t pos;
    };

    mmap_t mapRelationIndex;
    mmap_t mapRelationData;

};


#endif

