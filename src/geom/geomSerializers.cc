
#include <math.h>

#include "geomSerializers.h"
#include "genericGeometry.h"
#include "misc/symbolicNames.h"




void serializePolygon(const Ring &poly, const TTags &tags, uint64_t relId, FILE* fOut)
{
    //cerr << "serializing relation " << relId << endl;
    uint64_t sizeTmp = 
                    sizeof(uint8_t)  + // 'type' field
                    sizeof(uint64_t) + // 'id' field
                    sizeof(uint32_t) + // 'numTagBytes'
                    RawTags::getSerializedSize(tags) + //tags size
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
    
    RawTags::serialize(tags, fOut);
    
    uint32_t numRings = 1 + poly.children.size(); // 1 outer, plus the inner rings
    MUST(fwrite( &numRings, sizeof(uint32_t), 1, fOut) == 1, "write error");
    
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
        
    //cerr << "serializing relation " << relId << endl;
    //Tags tags(way.);
    TTags tags;

    for (std::pair<const char*, const char*> kv : way.getTags())
        tags.push_back( std::make_pair( kv.first, kv.second));
    uint64_t sizeTmp = 
                    sizeof(uint8_t)  + // 'type' field
                    sizeof(uint64_t) + // 'id' field
                    sizeof(uint32_t) + // 'numTagBytes'
                    RawTags::getSerializedSize(tags) + //tags size
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
    MUST(fwrite( &way.id, sizeof(way.id), 1, fOut) == 1, "write error");
    
    RawTags::serialize(tags, fOut);

    if (asPolygon)
    {    
        uint32_t numRings = 1; // ways only consist of a single outer ring (no inner rings)
        MUST(fwrite( &numRings, sizeof(uint32_t), 1, fOut) == 1, "write error");
    }

    uint32_t numVertices = way.numVertices;
    MUST(fwrite( &numVertices, sizeof(uint32_t), 1, fOut) == 1, "write error");
    
    for (int i = 0; i < way.numVertices; i++)
    {
        MUST(fwrite( &way.vertices[i].lat, sizeof(int32_t), 1, fOut) == 1, "write error");
        MUST(fwrite( &way.vertices[i].lng, sizeof(int32_t), 1, fOut) == 1, "write error");
    }       

    
    uint64_t endPos = ftell(fOut);
    //std::cout << "size calculated: " << numBytes << ", actual: " << (endPos - posBefore) 
    //          << std::endl;
    MUST( endPos == numBytes + posBefore, " polygon size mismatch");
}

