
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

class Tags{
public: 
 Tags( const char* src, int numTags): src(src), numTags(numTags) {}
 class ConstTagIterator;
 
 
 ConstTagIterator begin() const { return ConstTagIterator(src, numTags);}
 ConstTagIterator end()   const { return ConstTagIterator(src, 0); }
 
    class ConstTagIterator {

    public:
        ConstTagIterator( const char* src, uint64_t numTagsLeft): src(src), numTagsLeft(numTagsLeft) 
        { 
            //std::cout<< "created tag iterator with " << numTagsLeft << "tags" << std::endl;
        
        }
        ConstTagIterator& operator++() {
            if (numTagsLeft == 0)
                assert(false && "reached end of container");
                
            const char* key = src;
            const char* value = key + strlen(src) + 1; //including zero-termination
            src = value + strlen(value) + 1;
            numTagsLeft -= 1;
            return (*this);
        }
        
        std::pair<const char*, const char*> operator *() {
            const char* key = src;
            const char* value = key + strlen(src) + 1; //including zero-termination
            return std::make_pair(key, value);
        }
        
        bool operator !=( ConstTagIterator &other) const {return this->numTagsLeft != other.numTagsLeft;}
    
    private:
        const char* src;
        uint64_t numTagsLeft;
    };

private:
    const char* src;
    uint64_t numTags;

};


#endif
