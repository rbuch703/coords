
#include "genericGeometry.h"



OpaqueOnDiskGeometry::OpaqueOnDiskGeometry(FILE* f) 
{
    MUST(fread( &this->numBytes, sizeof(uint32_t), 1, f) == 1, "feature read error");
    /* there should be no OSM geometry bigger than 100MiB; these are likely
     * invalid reads */
    MUST( this->numBytes < 100*1000*1000, "geometry read error");
    
    this->bytes = new uint8_t[this->numBytes];
    MUST(fread( this->bytes, this->numBytes, 1, f) == 1, "feature read error");
    
}

OpaqueOnDiskGeometry::~OpaqueOnDiskGeometry()
{
    delete [] this->bytes;
}


FEATURE_TYPE OpaqueOnDiskGeometry::getFeatureType() const 
{
    MUST( numBytes > 0, "corrupted on-disk geometry");
    return (FEATURE_TYPE)bytes[0];
}

OSM_ENTITY_TYPE OpaqueOnDiskGeometry::getEntityType() const 
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

uint64_t OpaqueOnDiskGeometry::getEntityId() const 
{
    MUST( numBytes >= 9, "corrupted on-disk geometry");
    uint64_t id = *((uint64_t*)(bytes+1));
    return id & ~IS_WAY_REFERENCE;
}

Envelope OpaqueOnDiskGeometry::getBounds() const 
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

Envelope OpaqueOnDiskGeometry::getLineBounds() const {
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

Envelope OpaqueOnDiskGeometry::getPolygonBounds() const 
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

