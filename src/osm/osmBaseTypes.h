
#ifndef OSM_BASE_TYPES_H
#define OSM_BASE_TYPES_H

#include <assert.h>

#include <string>
#include <string.h>
#include <map>

enum struct OSM_ENTITY_TYPE : uint8_t { NODE, WAY, RELATION, CHANGESET, OTHER };

typedef std::pair<std::string, std::string> OsmKeyValuePair;

typedef struct 
{
    uint64_t id;
    int32_t lat, lng;
} OsmGeoPosition;

bool operator==(const OsmGeoPosition &a, const OsmGeoPosition &b);
bool operator!=(const OsmGeoPosition &a, const OsmGeoPosition &b);
bool operator< (const OsmGeoPosition &a, const OsmGeoPosition &b); //just an arbitrary ordering for containers that need one

OsmGeoPosition operator-(const OsmGeoPosition &a, const OsmGeoPosition &b);

struct OsmRelationMember
{
    OsmRelationMember( OSM_ENTITY_TYPE member_type, 
                       uint64_t member_ref, 
                       std::string member_role);

    uint32_t getDataSize() const;
    OSM_ENTITY_TYPE type;  //whether the member is a node, way or relation
    uint64_t ref;  //the node/way/relation id
    std::string role;   //the role the member has as part of the relation
    
};

template <typename T>
class ArrayIterator {
public:
    ArrayIterator(T* begin, T* end): beginPos(begin), endPos(end) {};
    
    T* begin() const { return beginPos;}
    T* end() const   { return endPos;}
private:
    T *beginPos, *endPos;
};

template <typename T>
class ConstArrayIterator {
public:
    ConstArrayIterator(const T* begin, const T* end): beginPos(begin), endPos(end) {};
    
    const T* begin() const { return beginPos;}
    const T* end() const   { return endPos;}
private:
    const T *beginPos, *endPos;
};

#endif
