
#include <math.h>

#include "geomSerializers.h"
#include "genericGeometry.h"
#include "misc/symbolicNames.h"
#include "misc/varInt.h"




void serializePolygon(const Ring &poly, const Tags &tags, uint64_t relId, FILE* fOut)
{
    //cerr << "serializing relation " << relId << endl;
    uint32_t numRings = 1 + poly.children.size(); // 1 outer, plus the inner rings

    uint64_t tagsSize = RawTags::getSerializedSize(tags);
    uint64_t sizeTmp = 
                    sizeof(uint8_t)  + // 'type' field
                    sizeof(uint64_t) + // 'id' field
                    varUintNumBytes(tagsSize) + 
                    tagsSize + //tags size
                    varUintNumBytes(numRings) +   // 'numRings' field
                    poly.getSerializedSize(false); // outer ring size

    for (const Ring* inner : poly.children)
        sizeTmp += inner->getSerializedSize(true);
                    
    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    uint64_t posBefore = ftell(fOut);
    
    FEATURE_TYPE et = FEATURE_TYPE::POLYGON;
    MUST(fwrite( &et,    sizeof(et),    1, fOut) == 1, "write error");
    MUST(fwrite( &relId, sizeof(relId), 1, fOut) == 1, "write error");
    
    RawTags::serialize(tags, fOut);
    varUintToFile(numRings, fOut);
    poly.serialize(fOut, false);

    for (const Ring* inner : poly.children)
        inner->serialize(fOut, true);
    
    uint64_t endPos = ftell(fOut);
    //cout << "size calculated: " << numBytes << ", actual: " << (endPos - posBefore) << endl;
    MUST( endPos == numBytes + posBefore, " polygon size mismatch");
}


void serializeWayAsGeometry(const OsmLightweightWay &way, bool asPolygon, FILE* fOut)
{
    OsmGeoPosition v0 = way.vertices[0];
    OsmGeoPosition vn = way.vertices[way.numVertices-1];
    
    if (asPolygon)
        MUST( v0.lat == vn.lat && v0.lng == vn.lng, "not a ring");
        
    Tags tags;

    for (std::pair<const char*, const char*> kv : way.getTags())
        tags.push_back( std::make_pair( kv.first, kv.second));
    uint64_t tagsSize = RawTags::getSerializedSize(tags);
    
    uint64_t sizeTmp = 
        sizeof(uint8_t)  + // 'type' field
        sizeof(uint64_t) + // 'id' field
        varUintNumBytes(tagsSize) +
        tagsSize + //tags size
        varUintNumBytes(way.numVertices);
        
    int64_t prevLat = 0;
    int64_t prevLng = 0;
    for (int i = 0; i < way.numVertices; i++)
    {
        int64_t dLat = way.vertices[i].lat - prevLat;
        int64_t dLng = way.vertices[i].lng - prevLng;
        prevLat = way.vertices[i].lat;
        prevLng = way.vertices[i].lng;
        
        sizeTmp += varUintNumBytes(dLat);
        sizeTmp += varUintNumBytes(dLng);
    }
                    
    if (asPolygon)
        sizeTmp += varUintNumBytes(1);  // 'numRings' field (="1")

    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    uint64_t posBefore = ftell(fOut);
    
    FEATURE_TYPE ft = asPolygon ? FEATURE_TYPE::POLYGON : FEATURE_TYPE::LINE;
    MUST(fwrite( &ft,    sizeof(ft),    1, fOut) == 1, "write error");
    MUST(fwrite( &way.id, sizeof(way.id), 1, fOut) == 1, "write error");
    
    RawTags::serialize(tags, fOut);

    if (asPolygon)
        varUintToFile(1, fOut); // ways only consist of a single outer ring (no inner rings)

    varUintToFile(way.numVertices, fOut);
    
    prevLat = 0;
    prevLng = 0;
    for (int i = 0; i < way.numVertices; i++)
    {
        int64_t dLat = way.vertices[i].lat - prevLat;
        int64_t dLng = way.vertices[i].lng - prevLng;
        prevLat = way.vertices[i].lat;
        prevLng = way.vertices[i].lng;
        
        varIntToFile( dLat, fOut);
        varIntToFile( dLng, fOut);
    }       

    
    uint64_t endPos = ftell(fOut);
    //std::cout << "size calculated: " << numBytes << ", actual: " << (endPos - posBefore) 
    //          << std::endl;
    MUST( endPos == numBytes + posBefore, " polygon size mismatch");
}

