
#include "geomSerializers.h"
#include "genericGeometry.h"

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


void serializeWayAsGeometry(const OsmLightweightWay &way, bool asPolygon, FILE* fOut)
{
    OsmGeoPosition v0 = way.vertices[0];
    OsmGeoPosition vn = way.vertices[way.numVertices-1];
    
    if (asPolygon)
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
    MUST(fwrite( &way.id, sizeof(way.id), 1, fOut) == 1, "write error");
    
    serializeTagSet(tags, fOut);

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

