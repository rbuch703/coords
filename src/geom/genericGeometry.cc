
#include "genericGeometry.h"

uint64_t getSerializedSize(const TagSet &tagSet); //forward


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


void serializePolygon(const Ring &poly, const TagSet &tags, uint64_t relId, FILE* fOut)
{
    //cerr << "serializing relation " << relId << endl;
    uint64_t sizeTmp = 
                    sizeof(uint8_t)  + // 'type' field
                    sizeof(uint64_t) + // 'id' field
                    sizeof(uint32_t) + // 'numTagBytes'
                    getSerializedSize(tags) + //tags size
                    sizeof(uint32_t) +   // 'numRings' field
                    poly.getSerializedSize(); // outer ring size

    for (const Ring* inner : poly.children)
        sizeTmp += inner->getSerializedSize();
                    
    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    uint64_t posBefore = ftell(fOut);
    
    FEATURE_TYPE et = FEATURE_TYPE::POLYGON;
    MUST(fwrite( &et,    sizeof(et),    1, fOut) == 1, "write error");
    MUST(fwrite( &relId, sizeof(relId), 1, fOut) == 1, "write error");
    
    serializeTagSet(tags, fOut);
    
    uint32_t numRings = 1 + poly.children.size(); // 1 outer, plus the inner rings
    MUST(fwrite( &numRings, sizeof(uint32_t), 1, fOut) == 1, "write error");
    
    poly.serialize(fOut, false);

    for (const Ring* inner : poly.children)
        inner->serialize(fOut, true);
    
    uint64_t endPos = ftell(fOut);
    //cout << "size calculated: " << numBytes << ", actual: " << (endPos - posBefore) << endl;
    MUST( endPos == numBytes + posBefore, " polygon size mismatch");
}


void serializeWay(const OsmLightweightWay &way, uint64_t wayId, bool asPolygon, FILE* fOut)
{
    OsmGeoPosition v0 = way.vertices[0];
    OsmGeoPosition vn = way.vertices[way.numVertices-1];
    
    MUST( v0.lat == vn.lat && v0.lng == vn.lng, "not a ring");
    //cerr << "serializing relation " << relId << endl;
    TagSet tags(way.getTagSet());
    uint64_t sizeTmp = 
                    sizeof(uint8_t)  + // 'type' field
                    sizeof(uint64_t) + // 'id' field
                    sizeof(uint32_t) + // 'numTagBytes'
                    getSerializedSize(tags) + //tags size
                    sizeof(uint32_t) +   // numPoints
                    sizeof(int32_t)*2* way.numVertices;
                    
    if (asPolygon)
        sizeTmp += sizeof(uint32_t);  // 'numRings' field (="1")

    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    uint64_t posBefore = ftell(fOut);
    
    FEATURE_TYPE ft = asPolygon ? FEATURE_TYPE::POLYGON : FEATURE_TYPE::LINE;
    MUST(fwrite( &ft,    sizeof(ft),    1, fOut) == 1, "write error");
    MUST(fwrite( &wayId, sizeof(wayId), 1, fOut) == 1, "write error");
    
    serializeTagSet(tags, fOut);

    if (asPolygon)
    {    
        uint32_t numRings = 1; // ways only consist of a single outer ring (no inner rings)
        MUST(fwrite( &numRings, sizeof(uint32_t), 1, fOut) == 1, "write error");
    }

    for (int i = 0; i < way.numVertices; i++)
    {
        MUST(fwrite( &way.vertices[i].lat, sizeof(int32_t), 1, fOut) == 1, "write error");
        MUST(fwrite( &way.vertices[i].lng, sizeof(int32_t), 1, fOut) == 1, "write error");
    }        

    
    uint64_t endPos = ftell(fOut);
    //cout << "size calculated: " << numBytes << ", actual: " << (endPos - posBefore) << endl;
    MUST( endPos == numBytes + posBefore, " polygon size mismatch");
}

uint64_t getSerializedSize(const TagSet &tagSet)
{
    uint64_t numTags = tagSet.size();
    uint64_t numNames = numTags * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames +7) / 8; //one bit per name --> have to round up
    
    uint64_t numTagBytes = 0;
    for (OsmKeyValuePair kv : tagSet)
        numTagBytes += (kv.first.length() + 1) + (kv.second.length() + 1);
    
    return sizeof(uint16_t) + 
           bitfieldSize * sizeof(uint8_t) +
           numTagBytes;

}

void serializeTagSet(const TagSet & tagSet, FILE* fOut)
{
    uint64_t tmp = getSerializedSize(tagSet);
    MUST(tmp < (1ull)<<32, "tag set size overflow");
    uint32_t numBytes = tmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    int64_t filePos = ftell(fOut);

    MUST( tagSet.size() < 1<<16, "attribute count overflow");
    uint16_t numTags = tagSet.size();
    MUST(fwrite( &numTags,  sizeof(numTags),  1, fOut) == 1, "write error");

    uint64_t numNames = numTags * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames +7) / 8; //one bit per name --> have to round up
    while (bitfieldSize--)
    {
        uint8_t bf = 0;
        MUST(fwrite( &bf, sizeof(bf), 1, fOut) == 1, "write error");
    }

    for (OsmKeyValuePair kv : tagSet)
    {
        const char* key = kv.first.c_str();
        MUST( fwrite( key, strlen(key)+1, 1, fOut) == 1, "write error");

        const char* val = kv.second.c_str();
        MUST( fwrite( val, strlen(val)+1, 1, fOut) == 1, "write error");
    }
    
    MUST( ftell(fOut)- numBytes == filePos, "tag set size mismatch");
    //cout << "tag set size calculated: " << numBytes << ", actual: " << (ftell(fOut) - filePos) << endl;
    
}

