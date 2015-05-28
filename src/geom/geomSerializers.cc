
#include <math.h>

#include "geomSerializers.h"
#include "genericGeometry.h"
#include "misc/symbolicNames.h"
#include "misc/varInt.h"

#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateSequenceFactory.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LineString.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/Point.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/PrecisionModel.h>


/* Note: all geometry data is stored as integers. Having the computations be performed
         using arbitrary floats can lead to subtle errors (simple polygons with non-connected
         interior, unclosed rings, ...) when later rounding those floats to integers. 
         To prevent those, we instruct geos to keep all coordinates as integers in the first
         place. */
static geos::geom::PrecisionModel model( geos::geom::PrecisionModel::Type::FIXED);
static geos::geom::GeometryFactory factory(&model);


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

GenericGeometry serializeWay(uint64_t wayId, OsmGeoPosition* vertices, uint64_t numVertices, 
                            const TagDictionary &wayTags, bool asPolygon)
{
    OsmGeoPosition v0 = vertices[0];
    OsmGeoPosition vn = vertices[numVertices-1];
    
    if (asPolygon)
        MUST( v0.lat == vn.lat && v0.lng == vn.lng, "polygon ring is not closed");
        
    Tags tags(wayTags.begin(), wayTags.end());

    /*for (const OsmKeyValuePair &kv : wayTags)
        tags.push_back( std::make_pair( kv.first, kv.second));*/
        
    uint64_t tagsSize = RawTags::getSerializedSize(tags);
    
    uint64_t sizeTmp = 
        sizeof(uint8_t)  + // 'type' field
        sizeof(uint64_t) + // 'id' field
        varUintNumBytes(tagsSize) +
        tagsSize + //tags size
        varUintNumBytes(numVertices);
        
    int64_t prevLat = 0;
    int64_t prevLng = 0;
    for (uint64_t i = 0; i < numVertices; i++)
    {
        int64_t dLat = vertices[i].lat - prevLat;
        int64_t dLng = vertices[i].lng - prevLng;
        prevLat = vertices[i].lat;
        prevLng = vertices[i].lng;
        
        sizeTmp += varIntNumBytes(dLat);
        sizeTmp += varIntNumBytes(dLng);
    }
                    
    if (asPolygon)
        sizeTmp += varUintNumBytes(1);  // 'numRings' field (="1")

    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    //uint32_t numBytesIncludingSizeField = numBytes + sizeof(uint32_t);
    
    uint8_t *outBuf = new uint8_t[numBytes];
    uint8_t *outPos = outBuf;
    
    //*(uint32_t*)outPos = numBytes;
    //outPos += sizeof(uint32_t);
    
    uint8_t *outStart = outPos;
    
    FEATURE_TYPE ft = asPolygon ? FEATURE_TYPE::POLYGON : FEATURE_TYPE::LINE;
    
    *(FEATURE_TYPE*)outPos = ft;
    outPos += sizeof(FEATURE_TYPE);
    
    *(uint64_t*)outPos = wayId;
    outPos += sizeof(uint64_t);

    uint64_t numTagBytes = 0;
    uint8_t * tagBytes = RawTags::serialize(tags, &numTagBytes);
    memcpy(outPos, tagBytes, numTagBytes);
    delete [] tagBytes;
    outPos += numTagBytes;

    if (asPolygon)
        // ways only consist of a single outer ring (no inner rings)
        outPos += varUintToBytes(1, outPos); 

    outPos += varUintToBytes(numVertices, outPos);
    
    prevLat = 0;
    prevLng = 0;
    for (uint64_t i = 0; i < numVertices; i++)
    {
        int64_t dLat = vertices[i].lat - prevLat;
        int64_t dLng = vertices[i].lng - prevLng;
        prevLat = vertices[i].lat;
        prevLng = vertices[i].lng;
        
        outPos += varIntToBytes( dLat, outPos);
        outPos += varIntToBytes( dLng, outPos);
    }       

    MUST( outPos - outStart == numBytes, " polygon size mismatch");
    return GenericGeometry(outBuf, numBytes, true);
}


static geos::geom::CoordinateSequence* getCoordinateSequence(const uint8_t* &pos)
{
    int nRead = 0;
    uint64_t numPoints = varUintFromBytes(pos, &nRead);
    pos += nRead;
    geos::geom::CoordinateSequence *seq = 
        factory.getCoordinateSequenceFactory()->create( (size_t)0, 2);    //start with 0 members; each member will have 2 dimensions
        
    MUST( numPoints < 10000000, "overflow"); //not a hard limit, but a sanity check
    int64_t x = 0;
    int64_t y = 0;
    while (numPoints--)
    {
        x += varIntFromBytes(pos, &nRead);
        pos += nRead;
        y += varIntFromBytes(pos, &nRead);
        pos += nRead;
        seq->add(geos::geom::Coordinate(x, y));
    }
    
    return seq;
}

static geos::geom::LineString* getGeosLine(const GenericGeometry &geom)
{
    const uint8_t *pos = geom.getGeometryPtr();
    return factory.createLineString(getCoordinateSequence(pos));
}

static geos::geom::Polygon* getGeosPolygon(const GenericGeometry &geom)
{
    const uint8_t* pos = geom.getGeometryPtr();
    int nRead = 0;
    uint64_t numRings = varUintFromBytes(pos, &nRead);
    pos += nRead;
    MUST(numRings > 0, "invalid polygon"); //must have at least an outer ring
    
    geos::geom::LinearRing *outer = factory.createLinearRing(getCoordinateSequence(pos));
    auto holes = new std::vector<geos::geom::Geometry*>();
    numRings-= 1;
    
    while ( numRings--)
    {
        holes->push_back(factory.createLinearRing(getCoordinateSequence(pos)));
    }
    
    //nothing to cleanup, createPolygon tages ownership of its arguments
    return factory.createPolygon( outer, holes);
}


geos::geom::Geometry* createGeosGeometry( const GenericGeometry &geom)
{
    switch (geom.getFeatureType())
    {
        case FEATURE_TYPE::POINT: 
        {
            const int32_t* pos = (const int32_t*)geom.getGeometryPtr();
            return factory.createPoint(geos::geom::Coordinate(pos[0], pos[1]));
            break;
        }
        case FEATURE_TYPE::LINE:  
            return getGeosLine(geom);
        case FEATURE_TYPE::POLYGON: 
            return getGeosPolygon(geom);
        default:
            MUST(false, "invalid feature type");
    }

    
}

void serializeWayAsGeometry(uint64_t wayId, OsmGeoPosition* vertices, uint64_t numVertices, const TagDictionary &wayTags, bool asPolygon, FILE* fOut)
{
    GenericGeometry geom = serializeWay(wayId, vertices, numVertices, wayTags, asPolygon);
    MUST( fwrite( geom.bytes, geom.numBytes, 1, fOut) == 1, "write error");
}

