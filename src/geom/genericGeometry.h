
#ifndef GENERIC_GEOMETRY_H
#define GENERIC_GEOMETRY_H

#include "config.h"
#include "osm/osmTypes.h"
#include "geom/envelope.h"
/* on-disk layout for tags:
    uint32_t numBytes
    uint16_t numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/

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
enum struct FEATURE_TYPE: uint8_t {POINT, LINE, POLYGON};

class OpaqueOnDiskGeometry {
public:
    FEATURE_TYPE getFeatureType() const {
        MUST( numBytes > 0, "corrupted on-disk geometry");
        return (FEATURE_TYPE)bytes[0];
    }
    
    OSM_ENTITY_TYPE getEntityType() const {
        MUST( numBytes >= 9, "corrupted on-disk geometry");
        uint64_t id = *((uint64_t*)bytes+1);
        switch (getFeatureType())
        {
            case FEATURE_TYPE::POINT: return OSM_ENTITY_TYPE::NODE;
            case FEATURE_TYPE::LINE:  return OSM_ENTITY_TYPE::WAY;
            case FEATURE_TYPE::POLYGON:
                return (id & IS_WAY_REFERENCE) ? OSM_ENTITY_TYPE::WAY :
                                                 OSM_ENTITY_TYPE::RELATION;
                break;
            default:
                MUST(false, "invalid FEATURE_TYPE");
                break;
        }
    }
    
    uint64_t getEntityId() const {
        MUST( numBytes >= 9, "corrupted on-disk geometry");
        uint64_t id = *((uint64_t*)bytes+1);
        return id & ~IS_WAY_REFERENCE;
    }
    
    Envelope getBounds() const {
        const int32_t *pos = (const int32_t*)(bytes + sizeof(uint8_t) + sizeof(uint64_t));
        switch (getFeatureType())
        {
            case FEATURE_TYPE::POINT: return Envelope( pos[0], pos[1]);
            case FEATURE_TYPE::LINE:  return getLineBounds();
            case FEATURE_TYPE::POLYGON: return getPolygonBounds();
            default: MUST(false, "invalid feature type"); break;
        }
    }
private:
    Envelope getLineBounds() const {
        uint8_t *beyond = bytes + numBytes;
        
        //skipping beyond 'type' and 'id'
        uint8_t* tagsStart = bytes + sizeof(uint8_t) + sizeof(uint64_t);
        uint32_t numTagBytes = *(uint32_t*)tagsStart;
        uint32_t *lineStart = (uint32_t*)tagsStart + sizeof(uint32_t) + numTagBytes;
        uint32_t numPoints = *lineStart;
        
        Envelope env;
        int32_t* points = (int32_t*)(lineStart + 1);
        while (numPoints--)
        {
            MUST(points + 2 <= (int32_t*)beyond, "out-of-bounds");
            env.add( points[0], points[1]);
            points += 2;
        }
        
        return env;
    }
    
    Envelope getPolygonBounds() const {
        uint8_t *beyond = bytes + numBytes;
        
        //skipping beyond 'type' and 'id'
        uint8_t* tagsStart = bytes + sizeof(uint8_t) + sizeof(uint64_t);
        uint32_t numTagBytes = *(uint32_t*)tagsStart;

        uint32_t *ringsStart = (uint32_t*)tagsStart + sizeof(uint32_t) + numTagBytes;
      
        uint32_t numRings = *ringsStart;

        Envelope env;
        
        uint32_t *lineStart = ringsStart +1;
        while (numRings--)        
        {
            uint32_t numPoints = *lineStart;
            int32_t* points = (int32_t*)(lineStart + 1);
            lineStart += (1 + numPoints);
            
            while (numPoints)
            {
                MUST(points + 2 <= (int32_t*)beyond, "out-of-bounds");
                env.add( points[0], points[1]);
                points += 2;
            }
        }
        return env;
    
    }
public:
    uint32_t numBytes;
    uint8_t *bytes;
    
        
};

#endif

