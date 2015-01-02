
#ifndef OSM_MAPPED_TYPES
#define OSM_MAPPED_TYPES

/* This file contains classes to work with OSM entities that are stored in memory-mapped files
*/

#include "osmTypes.h"


class OsmLightweightWay {
public:
    OsmLightweightWay( FILE* src, uint64_t way_id = -1);
    OsmLightweightWay( uint8_t* data_ptr, uint64_t way_id);
    ~OsmLightweightWay ();

    //disable copy constructor and assignment operator
    OsmLightweightWay( const OsmLightweightWay &other); 
    OsmLightweightWay &operator=(const OsmLightweightWay &other);

    void serialize( FILE* data_file/*, mmap_t *index_map*/) const;
    
    /** true when 'tagBytes' and 'vertices' point to areas inside a memory map,
        and thus any changes to 'tagBytes' and 'vertices' will directly change
        the data in the underlying file */
    bool     isDataMapped; 
    
    OsmGeoPosition *vertices;
    /*ways are guaranteed to have no more than 2000 nodes by the OSM specs, 
      so a uint16_t is sufficient */
    uint16_t numVertices; 

    uint8_t *tagBytes;
    uint32_t numTagBytes;
    uint16_t numTags;
    
    uint64_t id;
};

std::ostream& operator<<(std::ostream &out, const OsmLightweightWay &way);

class LightweightWayStore {

public:
    LightweightWayStore(const char* indexFileName, const char* dataFileName);
    OsmLightweightWay operator[](uint64_t wayId);
    bool exists(uint64_t wayId) const;

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