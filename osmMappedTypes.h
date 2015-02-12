
#ifndef OSM_MAPPED_TYPES
#define OSM_MAPPED_TYPES

/* This file contains classes to work with OSM entities that are stored in memory-mapped files
*/

#include "osmTypes.h"

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


class OsmLightweightWay {
public:
    OsmLightweightWay();
    OsmLightweightWay( FILE* src);
    OsmLightweightWay( uint8_t* data_ptr);
    OsmLightweightWay( const OsmLightweightWay &other);
    OsmLightweightWay( const OSMWay &other);

    ~OsmLightweightWay ();

    OsmLightweightWay &operator=(const OsmLightweightWay &other);
    OSMWay toOsmWay() const;
    void serialize( FILE* data_file/*, mmap_t *index_map*/) const;
    /* modify data without changing content, to make the underlying pages dirty
     * and force a writeback. This is mostly used to force linear streaming.
       writes of the backing mmap, instead of the much slower random access writes */
    void touch();
    std::map<std::string, std::string> getTags() const;
    
    uint64_t size() const;
    bool hasKey(const char* key) const;
    std::string getValue(const char* key) const;
    /** true when 'tagBytes' and 'vertices' point to areas inside a memory map,
        and thus any changes to 'tagBytes' and 'vertices' will directly change
        the data in the underlying file */

    ArrayIterator<OsmGeoPosition> getVertices() { return ArrayIterator<OsmGeoPosition>(vertices, vertices + numVertices);}    

    ConstArrayIterator<OsmGeoPosition> getVertices() const { return ConstArrayIterator<OsmGeoPosition>(vertices, vertices + numVertices);}    

    bool     isDataMapped; 
    OsmGeoPosition *vertices;
    /*ways are guaranteed to have no more than 2000 nodes by the OSM specs, 
      so a uint16_t is sufficient */
    uint16_t numVertices; 

    uint8_t *tagBytes;
    uint32_t numTagBytes;
    uint16_t numTags;
    
    uint64_t id;
    uint32_t version;
};

std::ostream& operator<<(std::ostream &out, const OsmLightweightWay &way);

class LightweightWayStore {

public:
    LightweightWayStore(const char* indexFileName, const char* dataFileName);
    OsmLightweightWay operator[](uint64_t wayId);
    bool exists(uint64_t wayId) const;
    void syncRange(uint64_t lowWayId, uint64_t highWayId) const ;

private:
    class LightweightWayIterator;

public:
    
    LightweightWayIterator begin();
    LightweightWayIterator end();

    /** \returns:
     *       an upper bound for the number of ways, which is also an upper bound for the
     *       highest wayId present in the data.  
    */
    uint64_t getMaxNumWays() const;

private:
    class LightweightWayIterator {

    public:
        LightweightWayIterator( LightweightWayStore &host, uint64_t pos);
        LightweightWayIterator& operator++();
        OsmLightweightWay operator *();
        bool operator !=( LightweightWayIterator &other) const;
    
    private:
        void advanceToNextWay();
        LightweightWayStore &host;
        uint64_t pos;
    };

    mmap_t mapWayIndex;
    mmap_t mapWayData;
};

class RelationStore {
public:
    RelationStore(const char* indexFileName, const char* dataFileName);
    OsmRelation operator[](uint64_t relationId) const;    
    bool exists(uint64_t relationId) const;
    uint64_t getMaxNumRelations() const;
private:
    class RelationIterator;
public:
    
    RelationIterator begin();
    RelationIterator end();

private:
    class RelationIterator {
    public:
        RelationIterator( RelationStore &host, uint64_t pos);        
        RelationIterator& operator++();
        OsmRelation operator *();        
        bool operator !=( RelationIterator &other) const;    
    private:
        void advanceToNextRelation();    
        RelationStore &host;
        uint64_t pos;
    };

    mmap_t mapRelationIndex;
    mmap_t mapRelationData;

};


#endif
