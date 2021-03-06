
#include <math.h>

#include "config.h"
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


/* Note: all geometry data is stored as integers, but geometry processing occurs as floats
         (because GEOS does not work robustly when operating on ints, and will fail on some
		 OSM data). So all methods serializing GEOS data must take care to sensibly round
		 the float values. */

static geos::geom::GeometryFactory factory;


//forward declarations
static void serialize(const geos::geom::LineString *ring, bool reverseVertexOrder, FILE* fOut);
static uint64_t getSerializedSize(const geos::geom::LineString *ring, bool reverseVertexOrder);


void serializePolygon(const Ring &poly, const Tags &tags, uint64_t relId, int8_t zIndex, FILE* fOut)
{
    //cerr << "serializing relation " << relId << endl;
    uint32_t numRings = 1 + poly.children.size(); // 1 outer, plus the inner rings
    MUST( poly.getPolygon()->getNumInteriorRing() == 0, "Ring with inner ring.");

    uint64_t tagsSize = RawTags::getSerializedSize(tags);
    uint64_t sizeTmp = 
                    sizeof(uint8_t)  + // 'type' field
                    sizeof(int8_t)   + // 'zIndex' field
                    varUintNumBytes(relId) + // 'id' field
                    varUintNumBytes(tagsSize) + 
                    tagsSize + //tags size
                    varUintNumBytes(numRings) +   // 'numRings' field
                    getSerializedSize(poly.getPolygon()->getExteriorRing(), false); // outer ring size

    for (const Ring* inner : poly.children)
    {
        MUST( inner->getPolygon()->getNumInteriorRing() == 0, "Ring with inner ring.");
        sizeTmp += getSerializedSize( inner->getPolygon()->getExteriorRing(), true);
    }
                    
    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    uint64_t posBefore = ftell(fOut);
    
    GEOMETRY_FLAGS gf = GEOMETRY_FLAGS::RELATION_POLYGON;
    MUST(fwrite( &gf,    sizeof(gf),    1, fOut) == 1, "write error");
    MUST(fwrite( &zIndex,sizeof(zIndex),1, fOut) == 1, "write error");
    
    varUintToFile( relId, fOut);
    
    RawTags::serialize(tags, fOut);
    varUintToFile(numRings, fOut);
    MUST( poly.getPolygon()->getNumInteriorRing() == 0, "Ring with inner ring.");
    serialize(poly.getPolygon()->getExteriorRing(), false, fOut);

    for (const Ring* inner : poly.children)
    {
        MUST( inner->getPolygon()->getNumInteriorRing() == 0, "Ring with inner ring.");
        serialize( inner->getPolygon()->getExteriorRing(), true, fOut);

    }
    
    uint64_t endPos = ftell(fOut);
    //cout << "size calculated: " << numBytes << ", actual: " << (endPos - posBefore) << endl;
    MUST( endPos == numBytes + posBefore, " polygon size mismatch");
}

static void serialize(const geos::geom::LineString *ring, bool reverseVertexOrder, FILE* fOut)
{
    const std::vector<geos::geom::Coordinate>& coords = 
        *ring->getCoordinatesRO()->toVector();
    
    MUST(coords.size() >= 4, "ring has less than four vertices");
    varUintToFile(coords.size(), fOut);

    
    int64_t prevX = 0;
    int64_t prevY = 0;
    if (!reverseVertexOrder)
    {
        for (uint64_t i = 0; i < coords.size(); i++)
        {
            int64_t x = (int64_t)coords[i].x;
            int64_t y = (int64_t)coords[i].y;
            int64_t dX = x - prevX;
            int64_t dY = y - prevY;
            prevX = x;
            prevY = y;
            varIntToFile(dX, fOut);
            varIntToFile(dY, fOut);
        }
    } else
    {
        for (int64_t i = coords.size()-1; i >= 0; i--)
        {
            int64_t x = (int64_t)coords[i].x;
            int64_t y = (int64_t)coords[i].y;
            int64_t dX = x - prevX;
            int64_t dY = y - prevY;
            prevX = x;
            prevY = y;
            varIntToFile(dX, fOut);
            varIntToFile(dY, fOut);
        }
    }
}

static uint64_t serialize( const geos::geom::LineString *ring, bool reverseVertexOrder, uint8_t* const outputBuffer)
{
    const geos::geom::CoordinateSequence &coords = *ring->getCoordinatesRO();
    uint64_t numVertices = coords.size();
    //std::cout << "serializing ring with " << numVertices << " vertices" << std::endl;
    uint8_t* outPos = outputBuffer;
    int64_t prevX = 0;
    int64_t prevY = 0;
    
    outPos += varUintToBytes(numVertices, outPos);
    
    if (!reverseVertexOrder)
    {
        for (uint64_t i= 0; i < numVertices; i++)
        {
            int x = (int64_t)coords[i].x;
            int y = (int64_t)coords[i].y;
            int64_t dX = x - prevX;
            int64_t dY = y - prevY;
            outPos += varIntToBytes(dX, outPos);
            outPos += varIntToBytes(dY, outPos);
            prevX += dX;
            prevY += dY;
        }
    } else
    {
        for (int64_t i = numVertices; i >= 0; i--)
        {
            int x = (int64_t)coords[i].x;
            int y = (int64_t)coords[i].y;
            int64_t dX = x - prevX;
            int64_t dY = y - prevY;
            outPos += varIntToBytes(dX, outPos);
            outPos += varIntToBytes(dY, outPos);
            prevX += dX;
            prevY += dY;
        }
    
    }
    return outPos - outputBuffer;
}

static uint64_t getSerializedSize(const geos::geom::LineString *ring, bool reverseVertexOrder)
{
    const std::vector<geos::geom::Coordinate> &coords = 
        *ring->getCoordinatesRO()->toVector();

    uint64_t size = varUintNumBytes( coords.size() ); //size of 'numVertices' field;
    
    int64_t prevX = 0;
    int64_t prevY = 0;
    if (!reverseVertexOrder)
    {
        for (uint64_t i = 0; i < coords.size(); i++)
        {
            int64_t x = (int64_t)coords[i].x;
            int64_t y = (int64_t)coords[i].y;
            int64_t dX = x - prevX;
            int64_t dY = y - prevY;
            prevX = x;
            prevY = y;
            size += varIntNumBytes( dX );   //size of the delta-encoded vertices
            size += varIntNumBytes( dY );
        }
    } else
    {
        for (int64_t i = coords.size() - 1; i >= 0; i--)
        {
            int64_t x = (int64_t)coords[i].x;
            int64_t y = (int64_t)coords[i].y;

            int64_t dX = x - prevX;
            int64_t dY = y - prevY;
            prevX = x;
            prevY = y;
            size += varIntNumBytes( dX );
            size += varIntNumBytes( dY );
        }
    }
    
    return size;
}

GenericGeometry serializeWay(uint64_t wayId, const std::vector<OsmGeoPosition> &vertices, const uint8_t *tagBytes, uint64_t numTagBytes, bool asPolygon, int8_t zIndex)
{
    OsmGeoPosition v0 = vertices.front();
    OsmGeoPosition vn = vertices.back();
    
    if (asPolygon)
        MUST( v0.lat == vn.lat && v0.lng == vn.lng, "polygon ring is not closed");
        
    uint64_t sizeTmp = 
        sizeof(uint8_t)  + // 'type' field
        sizeof( int8_t)   + // 'zIndex' field
        varUintNumBytes(wayId) + // 'id' field
        numTagBytes +
        varUintNumBytes(vertices.size());
        
    int64_t prevLat = 0;
    int64_t prevLng = 0;
    for (const OsmGeoPosition &pos : vertices)
    {
        int64_t dLat = pos.lat - prevLat;
        int64_t dLng = pos.lng - prevLng;
        prevLat = pos.lat;
        prevLng = pos.lng;
        
        sizeTmp += varIntNumBytes(dLat);
        sizeTmp += varIntNumBytes(dLng);
    }
                    
    if (asPolygon)
        sizeTmp += varUintNumBytes(1);  // 'numRings' field (="1")

    MUST( sizeTmp < (1ull) <<  32, "polygon size overflow");
    uint32_t numBytes = sizeTmp;
    
    uint8_t *outBuf = new uint8_t[numBytes];
    uint8_t *outPos = outBuf;
    
    uint8_t *outStart = outPos;
    
    GEOMETRY_FLAGS gf = asPolygon ? GEOMETRY_FLAGS::WAY_POLYGON : GEOMETRY_FLAGS::LINE;
    
    *(GEOMETRY_FLAGS*)outPos = gf;
    outPos += sizeof(GEOMETRY_FLAGS);

    *(int8_t*)outPos = zIndex;
    outPos += sizeof(int8_t);

    outPos += varUintToBytes(wayId, outPos);

    memcpy(outPos, tagBytes, numTagBytes);
    outPos += numTagBytes;

    if (asPolygon)
        // ways only consist of a single outer ring (no inner rings)
        outPos += varUintToBytes(1, outPos); 

    outPos += varUintToBytes( vertices.size(), outPos);
    
    prevLat = 0;
    prevLng = 0;
    for (const OsmGeoPosition &pos : vertices)
    {
        int64_t dLat = pos.lat - prevLat;
        int64_t dLng = pos.lng - prevLng;
        prevLat = pos.lat;
        prevLng = pos.lng;
        
        outPos += varIntToBytes( dLat, outPos);
        outPos += varIntToBytes( dLng, outPos);
    }       

    MUST( outPos - outStart == numBytes, " polygon size mismatch");
    return GenericGeometry(outBuf, numBytes, true);

}


GenericGeometry serializeWay(const OsmWay &way, bool asPolygon, int8_t zIndex)
{
    uint64_t numTagBytes = 0;
    uint8_t *tagBytes = RawTags::serialize(way.tags, &numTagBytes);

    GenericGeometry res = serializeWay(way.id, way.refs, tagBytes, numTagBytes, asPolygon, zIndex);
    delete [] tagBytes;
    
    return res;
    
}

GenericGeometry serializeNode(const OsmNode &node, int8_t zIndex)
{
    uint64_t numTagBytes = 0;
    uint8_t* tagBytes = RawTags::serialize( node.tags, &numTagBytes);
    
    uint64_t numBytes = 
        sizeof(uint8_t)  + // 'type' field
        sizeof(int8_t) + // geometry flags 
        varUintNumBytes(node.id) + //'id' field
        sizeof(int32_t)  + // lat
        sizeof(int32_t)  + // lng
        numTagBytes;
        
    uint8_t *outBuf = new uint8_t[numBytes];
    uint8_t *outPos = outBuf;
    
    *(GEOMETRY_FLAGS*)outPos = GEOMETRY_FLAGS::POINT;
    outPos += sizeof(GEOMETRY_FLAGS);
    
    *(int8_t*)outPos = zIndex;
    outPos += sizeof(int8_t);

    outPos += varUintToBytes( node.id, outPos);
    //*(uint64_t*)outPos = node.id;
    //outPos += sizeof(uint64_t);

    memcpy(outPos, tagBytes, numTagBytes);
    outPos += numTagBytes;
    delete [] tagBytes;

    *(int32_t*)outPos = node.lat;
    outPos += sizeof(int32_t);
    
    *(int32_t*)outPos = node.lng;
    outPos += sizeof(int32_t);
    
    //std::cout << (outPos - outBuf) << ", " << numBytes << std::endl;
    MUST( uint64_t(outPos - outBuf) == numBytes, "node size mismatch");
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

static uint64_t getSerializedSize( const geos::geom::Polygon *polygon)
{
    uint64_t numInteriorRings = polygon->getNumInteriorRing();
    uint64_t nBytes = varUintNumBytes(numInteriorRings + 1); //plus one exterior ring
    
    nBytes += getSerializedSize( polygon->getExteriorRing(), false );
    
    for (uint64_t i = 0; i < numInteriorRings; i++)
        nBytes += getSerializedSize( polygon->getInteriorRingN(i), false);
        
    return nBytes;
    

}


static GenericGeometry serializePolygon(const geos::geom::Polygon* poly, 
                                 uint64_t id, GEOMETRY_FLAGS flags, int8_t zIndex, 
                                 const RawTags &tags)
{
    uint64_t tagsSize = tags.getSerializedSize();
    uint64_t size = sizeof(uint8_t) + //GEOMETRY_FLAGS
                    sizeof(int8_t)  + //zIndex
                    varUintNumBytes(id) + // id
                    varUintNumBytes(tagsSize) +
                    tagsSize;

    MUST(poly, "invalid conversion");
    //std::cerr << "serializing id=" << id << std::endl;
    
    size += getSerializedSize( poly );
    
    uint8_t *outputBuffer = new uint8_t[size];
    uint8_t *outPos = outputBuffer;
    outputBuffer[0] = (uint8_t)flags;
    outPos = outputBuffer + sizeof(uint8_t);
    
    *(int8_t*)outPos = zIndex;
    outPos += sizeof(int8_t);
    
    outPos += varUintToBytes(id, outPos);
    //*(uint64_t*)outPos = id;
    //outPos += sizeof(uint64_t);
    
    outPos += tags.serialize(outPos);
    
    uint64_t numInteriorRings = poly->getNumInteriorRing();
    outPos += varUintToBytes(numInteriorRings+1, outPos); //plus exterior ring

    /* FIXME: for Mapnik, the polygon outer ring orientation has to be the opposite of the
              orientation of the inner rings. Currently, this code assumes that said orientation
              fixing has been done before and so will not be attempted here. To improve 
              robustness, orientation testing and correction shoul dbe performed here as well (
              e.g. based on an signed polygon area test() */
              
    outPos += serialize( poly->getExteriorRing(), false, outPos );

    for (uint64_t i = 0; i < numInteriorRings; i++)
        outPos += serialize( poly->getInteriorRingN(i), false, outPos );
    
    MUST( outPos - outputBuffer == (int64_t)size, "serialization size mismatch");

    return GenericGeometry(outputBuffer, size, true);
}

static GenericGeometry serializeLineString(const geos::geom::LineString* line, 
                                 uint64_t id, GEOMETRY_FLAGS flags, int8_t zIndex, const RawTags &tags)
{
    uint64_t tagsSize = tags.getSerializedSize();
    uint64_t size = sizeof(uint8_t) + //GEOMETRY_FLAGS
                    sizeof(int8_t)  + // zIndex
                    varUintNumBytes(id) + // id
                    varUintNumBytes(tagsSize) +
                    tagsSize;

    MUST(line, "invalid conversion");
    MUST((int8_t)flags & (int8_t)GEOMETRY_FLAGS::LINE, "invalid line type");
    //std::cerr << "serializing id=" << id << std::endl;
    
    size += getSerializedSize( line, false );
    
    uint8_t *outputBuffer = new uint8_t[size];
    uint8_t *outPos = outputBuffer;
    outputBuffer[0] = (uint8_t)flags;
    outPos = outputBuffer + sizeof(uint8_t);
    
    *(int8_t*)outPos = zIndex;
    outPos += sizeof(int8_t);
    
    outPos += varUintToBytes(id, outPos);
    //*(uint64_t*)outPos = id;
    //outPos += sizeof(uint64_t);
    outPos += tags.serialize(outPos);
    outPos += serialize( line, false, outPos );

    MUST( outPos - outputBuffer == (int64_t)size, "serialization size mismatch");

    return GenericGeometry(outputBuffer, size, true);
    
}


GenericGeometry serialize(geos::geom::Geometry* geom, uint64_t id, GEOMETRY_FLAGS flags, int8_t zIndex, const RawTags &tags)
{
    flags = GEOMETRY_FLAGS( 0x03 & (int8_t)flags);
    
    if (flags == GEOMETRY_FLAGS::RELATION_POLYGON || flags == GEOMETRY_FLAGS::WAY_POLYGON)
    {
        MUST(  geom->getGeometryTypeId() == geos::geom::GeometryTypeId::GEOS_POLYGON, "geometry type mismatch");
        return serializePolygon(dynamic_cast<geos::geom::Polygon*>(geom), id, 
                                flags, zIndex, tags);
    }
    
    if (flags == GEOMETRY_FLAGS::LINE)
    {
        MUST( geom->getGeometryTypeId() == geos::geom::GeometryTypeId::GEOS_LINESTRING, "geometry type mismatch");
        return serializeLineString(dynamic_cast<geos::geom::LineString*>(geom), id, 
                                   flags, zIndex, tags);
    }

    MUST( false , "serializing GEOS geometries other than polygons and line strings"
                          " is not implemented");
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

geos::geom::Geometry* createGeosGeometry( const OsmWay &geom, bool asPolygon)
{
    geos::geom::CoordinateSequence *seq = 
        factory.getCoordinateSequenceFactory()->create( (size_t)0, 2);    //start with 0 members; each member will have 2 dimensions

    
    MUST( geom.refs.size() <= 2000, "overflow"); //OSM hard limit for nodes per way
    for (const OsmGeoPosition &pos : geom.refs)
        seq->add( geos::geom::Coordinate( pos.lat, pos.lng));

    if (asPolygon)
    {
        MUST( geom.refs.front().lat == geom.refs.back().lat &&
              geom.refs.front().lng == geom.refs.back().lng &&
              geom.refs.size() >= 4, "not a polygon");
        return factory.createPolygon( factory.createLinearRing(seq), nullptr);
    }

    return factory.createLineString( seq );
    
}

/*
void serializeWayAsGeometry(uint64_t wayId, OsmGeoPosition* vertices, uint64_t numVertices, const TagDictionary &wayTags, bool asPolygon, FILE* fOut)
{
    GenericGeometry geom = serializeWay(wayId, vertices, numVertices, wayTags, asPolygon);
    MUST( fwrite( geom.bytes, geom.numBytes, 1, fOut) == 1, "write error");
}
*/
