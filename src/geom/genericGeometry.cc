
#include <string.h>

#include "config.h"
#include "genericGeometry.h"

#ifndef COORDS_MAPNIK_PLUGIN
#include "misc/varInt.h"
#else
    #include "varInt.h"
#endif


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
                varInt lat;
                varInt lng;
    
*/

RawTags GenericGeometry::getTags() const
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(uint64_t);
    return RawTags(tagsStart);
}

const uint8_t* GenericGeometry::getGeometryPtr() const
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(uint64_t);
    
    int nRead = 0;
    uint32_t numTagBytes = varUintFromBytes(tagsStart, &nRead);
    uint8_t* geomStart = tagsStart + nRead + numTagBytes;
    MUST( geomStart < this->bytes + this->numBytes, "overflow");
    return geomStart;
}


GenericGeometry::GenericGeometry(FILE* f): numBytes(0), numBytesAllocated(0), bytes(nullptr)
{
    init(f, false);
}

#ifdef COORDS_MAPNIK_PLUGIN
GenericGeometry::GenericGeometry(): numBytes(0), numBytesAllocated(0), bytes(nullptr)
{
}
#endif


void GenericGeometry::init(FILE* f, bool avoidRealloc)
{
    MUST(fread( &this->numBytes, sizeof(uint32_t), 1, f) == 1, "feature read error");
    /* there should be no OSM geometry bigger than 50MiB; 
       such results are likely just invalid reads */
    MUST( this->numBytes < 50*1000*1000, "geometry read error");
    
    if (!avoidRealloc || this->numBytes > numBytesAllocated)
    {
        delete [] this->bytes;
        this->bytes = new uint8_t[this->numBytes];
        this->numBytesAllocated = this->numBytes;
    }
    
    MUST(fread( this->bytes, this->numBytes, 1, f) == 1, "feature read error");    
}



GenericGeometry::GenericGeometry(const GenericGeometry &other)
{
    if (this == &other)
        return;
        
    this->numBytes = other.numBytes;
    this->bytes = new uint8_t[this->numBytes];
    memcpy( this->bytes, other.bytes, this->numBytes);
}


GenericGeometry::~GenericGeometry()
{
    delete [] this->bytes;
}


FEATURE_TYPE GenericGeometry::getFeatureType() const 
{
    MUST( numBytes > 0, "corrupted on-disk geometry");
    return (FEATURE_TYPE)bytes[0];
}

OSM_ENTITY_TYPE GenericGeometry::getEntityType() const 
{
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

uint64_t GenericGeometry::getEntityId() const 
{
    MUST( numBytes >= 9, "corrupted on-disk geometry");
    uint64_t id = *((uint64_t*)(bytes+1));
    return id & ~IS_WAY_REFERENCE;
}

Envelope GenericGeometry::getBounds() const 
{
    const int32_t *pos = (const int32_t*)(bytes + sizeof(uint8_t) + sizeof(uint64_t));
    switch (getFeatureType())
    {
        case FEATURE_TYPE::POINT: return Envelope( pos[0], pos[1]);
        case FEATURE_TYPE::LINE:  return getLineBounds();
        case FEATURE_TYPE::POLYGON: return getPolygonBounds();
        default: MUST(false, "invalid feature type"); break;
    }
}

Envelope GenericGeometry::getLineBounds() const {
    uint8_t *beyond = bytes + numBytes;
    
    const uint8_t *lineStart = getGeometryPtr();
    int nBytes = 0;

    uint64_t numPoints = varUintFromBytes(lineStart, &nBytes);
    lineStart += nBytes;
    
    Envelope env;
    int64_t x = 0;
    int64_t y = 0;
    while (numPoints--)
    {
        int64_t dX = varIntFromBytes(lineStart, &nBytes);
        lineStart += nBytes;
        int64_t dY = varIntFromBytes(lineStart, &nBytes);
        lineStart += nBytes;
        x += dX;
        y += dY;
        MUST(lineStart <= beyond, "out-of-bounds");
        env.add( x, y);
    }
    
    MUST( lineStart == beyond, "geometry size mismatch")
    return env;
}

Envelope GenericGeometry::getPolygonBounds() const 
{
    uint8_t *beyond = bytes + numBytes;
    const uint8_t *ringStart = getGeometryPtr();
  
    int nBytes = 0;
    uint64_t numRings = varUintFromBytes(ringStart, &nBytes);
    ringStart += nBytes;

    Envelope env;
    
    while (numRings--)
    {
        uint32_t numPoints = varUintFromBytes(ringStart, &nBytes);
        ringStart += nBytes;
        
        int64_t x = 0;
        int64_t y = 0;
        while (numPoints--)
        {
            int64_t dX = varIntFromBytes(ringStart, &nBytes);
            ringStart += nBytes;
            int64_t dY = varIntFromBytes(ringStart, &nBytes);
            ringStart += nBytes;
            x += dX;
            y += dY;
            MUST(ringStart <= beyond, "out-of-bounds");
            env.add( x, y);
        }
    }
    
    MUST( ringStart == beyond, "geometry size mismatch");
    return env;

}


std::ostream& operator<<(std::ostream& os, FEATURE_TYPE ft)
{
    switch(ft) {
         case FEATURE_TYPE::POINT    : os << "POINT";    break;
         case FEATURE_TYPE::LINE     : os << "LINE";     break;
         case FEATURE_TYPE::POLYGON  : os << "POLYGON";  break;
         default : MUST(false, "invalid FEATURE_TYPE");
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, OSM_ENTITY_TYPE et)
{
    switch(et) {
         case OSM_ENTITY_TYPE::NODE     : os << "NODE";      break;
         case OSM_ENTITY_TYPE::WAY      : os << "WAY";       break;
         case OSM_ENTITY_TYPE::RELATION : os << "RELATION";  break;
         default : MUST(false, "invalid OSM_ENTITY_TYPE");
    }
    return os;
}

