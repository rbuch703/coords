
#ifndef GEOM_SERIALIZATION
#define GEOM_SERIALIZATION

#include "osm/osmMappedTypes.h"
#include "geom/ring.h"

#include <string>
#include <map>

typedef std::map<std::string, std::string> TagSet;
void     serializeTagSet(  const TagSet &tagSet, FILE* fOut);

void serializePolygon(const Ring &poly, const TagSet &tags, uint64_t relId, FILE* fOut);
void serializeWayAsGeometry(const OsmLightweightWay &way, bool asPolygon, FILE* fOut);


/* ON-DISK LAYOUT FOR TAGS:
    uint32_t numBytes
    uint16_t numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/


/* ON-DISK LAYOUT FOR GEOMETRY
:
    uint32_t size in bytes (not counting the size field itself)
    uint8_t  type ( 0 --> POINT, 1 --> LINE, 2 --> POLYGON)
    uint64_t id ( POINT--> nodeId; LINE--> wayId;
                  POLYGON:  MSB set --> wayId; MSB unset --> relationId)
    <tags>
    <type-specific data>:
    
   1. type == POINT
        int32_t lat;
        int32_t lng;
        
   2. type == LINE
        uint32_t numPoints;
        'numPoints' times:
            int32_t lat;
            int32_t lng;
    
   3. type == POLYGON
        uint32_t numRings;
        'numRings' times:
            uint32_t numPoints;
            'numPoints' times:
                int32_t lat;
                int32_t lng;
    
*/

#endif


