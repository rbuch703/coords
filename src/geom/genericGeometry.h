
#ifndef GENERIC_GEOMETRY_H
#define GENERIC_GEOMETRY_H

#include "config.h"
#include "osm/osmTypes.h"
#include "geom/envelope.h"
#include "geom/ring.h"

/* on-disk layout:
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

/* on-disk layout for tags:
    uint32_t numBytes
    uint16_t numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/

enum struct FEATURE_TYPE: uint8_t {POINT, LINE, POLYGON};
std::ostream& operator<<(std::ostream& os, FEATURE_TYPE ft);
std::ostream& operator<<(std::ostream& os, OSM_ENTITY_TYPE et);

typedef std::map<std::string, std::string> TagSet;
void     serializeTagSet(  const TagSet &tagSet, FILE* fOut);

void serializePolygon(const Ring &poly, const TagSet &tags, uint64_t relId, FILE* fOut);
void serializeWay(const OsmLightweightWay &way, bool asPolygon, FILE* fOut);


class OpaqueOnDiskGeometry {
public:
    OpaqueOnDiskGeometry(FILE* f);
    ~OpaqueOnDiskGeometry();
    
    FEATURE_TYPE getFeatureType() const;    //POINT/LINE/POLYGON
    OSM_ENTITY_TYPE getEntityType() const;  //NODE/WAY/RELATION
    uint64_t getEntityId() const;
    Envelope getBounds() const;    
private:
    Envelope getLineBounds() const;
    Envelope getPolygonBounds() const;
public:
    uint32_t numBytes;
    uint8_t *bytes;
    
        
};

#endif

