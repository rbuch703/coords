
#ifndef GEOM_SERIALIZATION
#define GEOM_SERIALIZATION

#include "osm/osmMappedTypes.h"
#include "geom/ring.h"
#include "misc/rawTags.h"

#include <string>
#include <map>


void serializePolygon(const Ring &poly, const Tags &tags, uint64_t relId, FILE* fOut);
void serializeWayAsGeometry(const OsmLightweightWay &way, bool asPolygon, FILE* fOut);




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
        varUint numPoints;
        'numPoints' times:
            varInt lat; //delta-encoded
            varInt lng; //delta-encoded
    
   3. type == POLYGON
        varUint numRings;
        'numRings' times:
            varUint numPoints;
            'numPoints' times:
                varInt lat; //delta-encoded
                varInt lng; //delta-encoded
    
*/

#endif


