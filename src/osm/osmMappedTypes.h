
#ifndef OSM_MAPPED_TYPES
#define OSM_MAPPED_TYPES

/* This file contains classes to work with OSM entities that are stored in memory-mapped files
*/

#include <assert.h>
//#include "containers/chunkedFile.h"
#include "config.h"
#include "osm/osmBaseTypes.h"
#include "misc/rawTags.h"
#include "geom/envelope.h"


class OsmLightweightWay {
public:
    OsmLightweightWay();
    OsmLightweightWay( FILE* src);
    OsmLightweightWay( uint8_t* data_ptr);
    OsmLightweightWay( const OsmLightweightWay &other);
//    OsmLightweightWay( const OsmWay &other);

    ~OsmLightweightWay ();

    OsmLightweightWay &operator=(const OsmLightweightWay &other);
//    OsmWay toOsmWay() const;
    void     serialize( FILE* data_file/*, mmap_t *index_map*/) const;
    uint8_t* serialize( uint8_t* dest) const;

    //std::map<std::string, std::string> getTagSet() const;
    RawTags getTags() const { return RawTags(tagBytes);}
    uint64_t size() const;
    bool hasKey(const char* key) const;
    std::string getValue(const char* key) const;
    Envelope getBounds() const;
    
    /* convert 'vertices' and 'tagBytes' to owned arrays from memory-mapped ranges. 
       After this call, changes to either one of these arrays no longer cause changes
       to the underlying file.
    */
    void unmap();


    ArrayIterator<OsmGeoPosition> getVertices() { return ArrayIterator<OsmGeoPosition>(vertices, vertices + numVertices);}    

    ConstArrayIterator<OsmGeoPosition> getVertices() const { return ConstArrayIterator<OsmGeoPosition>(vertices, vertices + numVertices);}    

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
    
    uint64_t id;
    uint32_t version;
};

std::ostream& operator<<(std::ostream &out, const OsmLightweightWay &way);



#endif
