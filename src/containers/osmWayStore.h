
#ifndef OSM_WAY_STORE_H
#define OSM_WAY_STORE_H

#include <stdint.h>
#include <string>
#include "osm/osmMappedTypes.h"
#include "misc/mem_map.h"

class LightweightWayStore {

public:
    LightweightWayStore(const char* indexFileName, const char* dataFileName, bool optimizeForStreaming = false);
    LightweightWayStore(const std::string &baseName, bool optimizeForStreaming = false);
    OsmLightweightWay operator[](uint64_t wayId);
    bool exists(uint64_t wayId) const;
    //void syncRange(uint64_t lowWayId, uint64_t highWayId) const ;

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


#endif

