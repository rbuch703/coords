
#include <string.h>

#include "config.h"
#include "genericGeometry.h"

#ifndef COORDS_MAPNIK_PLUGIN
#include "misc/varInt.h"
#else
    #include "varInt.h"
#endif


/* NOTE: for the on-disk layout of GenericGeometry, see geomSerializers.h */

RawTags GenericGeometry::getTags() const
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(int8_t);
    int nRead = 0;
    varUintFromBytes( tagsStart, &nRead);
    tagsStart += nRead;
    
    return RawTags(tagsStart);
}

const uint8_t* GenericGeometry::getGeometryPtr() const
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(int8_t);
    int nRead = 0;
    varUintFromBytes( tagsStart, &nRead);
    tagsStart += nRead;
    
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


GenericGeometry::GenericGeometry(uint8_t *bytes, uint32_t numBytes, bool takeOwnership)
{
    this->numBytes = numBytes;
    this->numBytesAllocated = numBytes;

    if (takeOwnership)
    {
        this->bytes = bytes;
    } else
    {
        this->bytes = new uint8_t[numBytes];
        memcpy( this->bytes, bytes, numBytes);
    }
}

GenericGeometry::GenericGeometry(const GenericGeometry &other)
{
    if (this == &other)
        return;
        
    this->numBytes = other.numBytes;
    this->bytes = new uint8_t[this->numBytes];
    memcpy( this->bytes, other.bytes, this->numBytes);
}

GenericGeometry::GenericGeometry(GenericGeometry &&other)
{
    //std::cout << "move constructor used" << std::endl;
    if (this == &other)
        return;
        
    this->numBytes = other.numBytes;
    this->bytes = other.bytes;
    
    other.numBytes = 0;
    other.bytes = nullptr;
}


GenericGeometry::~GenericGeometry()
{
    delete [] this->bytes;
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

void GenericGeometry::replaceTags( const TagDictionary &newTags)
{
    uint8_t* tagsStart = this->bytes + sizeof(uint8_t) + sizeof(int8_t);
    int nRead = 0;
    varUintFromBytes( tagsStart, &nRead);   //read past 'id' field
    tagsStart += nRead;
    
    uint32_t numTagBytes = varUintFromBytes(tagsStart, &nRead);
    uint8_t* geomStart = tagsStart + nRead + numTagBytes;
    MUST( geomStart < this->bytes + this->numBytes, "overflow");
    
    uint64_t numNewTagBytes = 0;
    Tags tagsTmp( newTags.begin(), newTags.end());
    uint8_t *newTagBytes = RawTags::serialize(tagsTmp, &numNewTagBytes);
    
    uint64_t newNumBytes = numBytes - (numTagBytes    + varUintNumBytes(numTagBytes))
                                    + (numNewTagBytes); //includes the size field itself
    
    uint8_t *newBytes = new uint8_t[newNumBytes];
    memcpy( newBytes, this->bytes, (tagsStart - this->bytes));
    uint8_t *newPos = newBytes +   (tagsStart - this->bytes);
    
    memcpy( newPos, newTagBytes, numNewTagBytes);
    delete [] newTagBytes;
    newPos += numNewTagBytes;

    uint64_t numNonGeoBytes =      (geomStart - this->bytes);
    memcpy( newPos, geomStart, this->numBytes - numNonGeoBytes);
    newPos += this->numBytes - numNonGeoBytes;
    
    MUST( newPos - newBytes == (int64_t)newNumBytes, "serialization size mismatch");
    delete [] this->bytes;
    
    this->bytes = newBytes;
    this->numBytes          = newNumBytes;
    this->numBytesAllocated = newNumBytes;
}




FEATURE_TYPE GenericGeometry::getFeatureType() const 
{
    MUST( numBytes > 0, "corrupted on-disk geometry");
    int8_t flags = (*(int8_t*)bytes) & 0x03; //lower two bits determine type
    
    switch ( (GEOMETRY_FLAGS)flags)
    {
        case GEOMETRY_FLAGS::POINT:return FEATURE_TYPE::POINT;
        case GEOMETRY_FLAGS::LINE: return FEATURE_TYPE::LINE;
        case GEOMETRY_FLAGS::WAY_POLYGON: return FEATURE_TYPE::POLYGON;
        case GEOMETRY_FLAGS::RELATION_POLYGON: return FEATURE_TYPE::POLYGON;
        default:
            MUST(false, "logic error");
            return (FEATURE_TYPE)-1;
    }
}

OSM_ENTITY_TYPE GenericGeometry::getEntityType() const 
{
    MUST( numBytes > 0, "corrupted on-disk geometry");
    int8_t flags = (*(int8_t*)bytes) & 0x03; //lower two bits determine type
    
    switch ( (GEOMETRY_FLAGS)flags)
    {
        case GEOMETRY_FLAGS::POINT:return OSM_ENTITY_TYPE::NODE;
        case GEOMETRY_FLAGS::LINE: return OSM_ENTITY_TYPE::WAY;
        case GEOMETRY_FLAGS::WAY_POLYGON: return OSM_ENTITY_TYPE::WAY;
        case GEOMETRY_FLAGS::RELATION_POLYGON: return OSM_ENTITY_TYPE::RELATION;
        default:
            MUST(false, "logic error");
            return (OSM_ENTITY_TYPE)-1;
    }
}

GEOMETRY_FLAGS GenericGeometry::getGeometryFlags() const 
{
    MUST( numBytes > 0, "corrupted on-disk geometry");
    return *(GEOMETRY_FLAGS*)bytes;
}

void GenericGeometry::serialize(FILE* f) const
{
    MUST( fwrite(&this->numBytes, sizeof(uint32_t), 1, f) == 1, "write error");
    MUST( fwrite(this->bytes, this->numBytes, 1, f) == 1, "write error");
}

uint64_t GenericGeometry::getEntityId() const 
{
    return varUintFromBytes(bytes + sizeof(uint8_t) + sizeof(int8_t), nullptr);
}

int8_t GenericGeometry::getZIndex() const 
{
    return *(int8_t*)(bytes + sizeof(uint8_t));
}


Envelope GenericGeometry::getBounds() const 
{
    const int32_t *pos = (const int32_t*) getGeometryPtr();
    switch (GEOMETRY_FLAGS((int8_t)getGeometryFlags() & 0x03))
    {
        case GEOMETRY_FLAGS::POINT: return Envelope( pos[0], pos[1]);
        case GEOMETRY_FLAGS::LINE:  return getLineBounds();
        case GEOMETRY_FLAGS::WAY_POLYGON:
        case GEOMETRY_FLAGS::RELATION_POLYGON:
            return getPolygonBounds();
        default: MUST(false, "invalid feature type"); return Envelope();
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

/*
bool GenericGeometry::hasMultipleRings() const
{
    if (this->getFeatureType() != FEATURE_TYPE::POLYGON)
        return false;
        
    const uint8_t* geo = getGeometryPtr();
    return varUintFromBytes( geo, nullptr) > 1;
}*/


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

