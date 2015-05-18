
#include "config.h"
#include "genericGeometry.h"

#include <string.h>

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

RawTags GenericGeometry::getTags() const
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(uint64_t);
/* ON-DISK LAYOUT FOR TAGS:
    uint32_t numBytes
    uint16_t numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/
    uint32_t numTagBytes = *(uint32_t*)tagsStart;
    //std::cout << "\thas " << numTagBytes << "b of tags."<< std::endl;
    tagsStart += sizeof(uint32_t);
    MUST( tagsStart + numTagBytes < this->bytes + this->numBytes, "overflow" );
    uint8_t *pos = tagsStart;
    
    uint16_t numTags = *(uint16_t*)pos;
    pos += sizeof(uint16_t);
    
    //uint8_t *symbolicNameBytes = pos;
    uint64_t numNames = numTags * 2; // key and value per tag
    uint64_t numSymbolicNameBytes = (numNames + 7) / 8;

        
    return RawTags(numTags, numTagBytes - numSymbolicNameBytes - sizeof(uint16_t), pos, pos + numSymbolicNameBytes);
}

#if 0
std::vector<Tag> GenericGeometry::getTags() const
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(uint64_t);
/* ON-DISK LAYOUT FOR TAGS:
    uint32_t numBytes
    uint16_t numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/
    uint32_t numTagBytes = *(uint32_t*)tagsStart;
    //std::cout << "\thas " << numTagBytes << "b of tags."<< std::endl;
    tagsStart += sizeof(uint32_t);
    MUST( tagsStart + numTagBytes < this->bytes + this->numBytes, "overflow" );
    uint8_t *pos = tagsStart;
    
    uint16_t numTags = *(uint16_t*)pos;
    pos += sizeof(uint16_t);
    
    //std::cout << "geometry has " << numTags << " tags" << std::endl;
    uint64_t numNames = numTags * 2;
    uint64_t numSymbolicNameBytes = (numNames + 7) / 8;
    while (numSymbolicNameBytes--)
    {
        MUST( *pos == 0, "symbolic tag storage not implemented");
        pos += 1;
    }
    
    std::vector<Tag> tags;
    while (numTags--)
    {
        MUST( pos < (tagsStart + numTagBytes), "overflow");
        std::string key = (char*)pos;
        pos += (key.length() + 1);
        
        std::string value=(char*)pos;
        pos += (value.length() + 1);
        
        tags.push_back( make_pair(key, value));
    }
    
    return tags;
}
#endif


uint8_t* GenericGeometry::getGeometryPtr()
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(uint64_t);
    
    uint32_t numTagBytes = *(uint32_t*)tagsStart;
    tagsStart += sizeof(uint32_t);
    
    uint8_t* geomStart =tagsStart + numTagBytes;
    MUST( geomStart < this->bytes + this->numBytes, "overflow");
    return geomStart;
}


GenericGeometry::GenericGeometry(FILE* f): numBytes(0), numBytesAllocated(0), bytes(nullptr)
{
    init(f, false);
}

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
    
    //skipping beyond 'type' and 'id'
    uint8_t* tagsStart = bytes + sizeof(uint8_t) + sizeof(uint64_t);
    uint32_t numTagBytes = *(uint32_t*)tagsStart;
    uint32_t *lineStart = (uint32_t*)(tagsStart + sizeof(uint32_t) + numTagBytes);
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

Envelope GenericGeometry::getPolygonBounds() const 
{
    uint8_t *beyond = bytes + numBytes;
    
    //skipping beyond 'type' and 'id'
    uint8_t* tagsStart = bytes + sizeof(uint8_t) + sizeof(uint64_t);
    uint32_t numTagBytes = *(uint32_t*)tagsStart;

    uint32_t *ringStart = (uint32_t*)(tagsStart + sizeof(uint32_t) + numTagBytes);
  
    uint32_t numRings = *ringStart;
    ringStart +=1;

    Envelope env;
    
    while (numRings--)
    {
        uint32_t numPoints = *ringStart;
        int32_t* points = (int32_t*)(ringStart + 1);
        
        while (numPoints--)
        {
            MUST(points + 2 <= (int32_t*)beyond, "out-of-bounds");
            env.add( points[0], points[1]);
            points += 2;
        }

        ringStart = (uint32_t*)points;

    }
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

