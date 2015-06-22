
#include <math.h>

#include "srsConversion.h"
#include "config.h"
#include "misc/varInt.h"

static const double INT_TO_LAT_LNG = 1/10000000.0;
static const double INT_TO_MERCATOR_METERS = 1/100.0;

static const double HALF_EARTH_CIRCUMFERENCE = 20037508.3428; // ~ 20037km
static const double PI = 3.141592653589793;

void convertWgs84ToWebMercator( int32_t &lat, int32_t &lng)
{
    MUST( lat >=  -900000000 && lat <=  900000000, "latitude out of range");
    MUST( lng >= -1800000000 && lng <= 1800000000, "longitude out of range");

    static const int MAX_MERCATOR_VALUE = HALF_EARTH_CIRCUMFERENCE / INT_TO_MERCATOR_METERS;

    int32_t x =  (lng * INT_TO_LAT_LNG) / 180 * MAX_MERCATOR_VALUE;
    MUST( x >= -MAX_MERCATOR_VALUE && x <= MAX_MERCATOR_VALUE, "projection overflow");
    
    int32_t y =  log(tan((90 + lat * INT_TO_LAT_LNG) * PI / 360)) / PI * MAX_MERCATOR_VALUE;
    if (y < -MAX_MERCATOR_VALUE) y = -MAX_MERCATOR_VALUE;
    if (y >  MAX_MERCATOR_VALUE) y =  MAX_MERCATOR_VALUE;
    
       
    lat = x;
    lng = y;
}


void convertWgs84ToWebMercator( OsmWay &way)
{
    for (OsmGeoPosition &pos : way.refs)
        convertWgs84ToWebMercator( pos.lat, pos.lng);
}

void convertWgs84ToWebMercator( GenericGeometry &geom)
{
    MUST( geom.getFeatureType() == FEATURE_TYPE::POLYGON, "not implemented");

    /* conversion changes value in 'geom' and - since those values are delta-encoded varInts - 
     * may change the size of 'geom'. So we need to allocate new storage for the output.
     * Since all 'geom' members compressed may grow at most by a factor of five (one byte before,
     * encoded (u)int32_t -> 5 bytes after), we need to allocate at most 5 times as much memory
     * as the original data structure is big. */
    uint64_t newAllocatedSize = geom.numBytes * 5;
    uint8_t *newData = new uint8_t[newAllocatedSize];
    uint8_t *basePtr = geom.bytes;
    const uint8_t *geoPtr  = geom.getGeometryPtr();
    int64_t baseSize = geoPtr - basePtr;
    MUST( baseSize > 0, "logic error");
    
    // copy that part of 'geom' that won't change through projection (id, type, tags)
    memcpy(newData, basePtr, baseSize);
    
    uint8_t *newGeoPtr = newData + baseSize;
    
    int nRead = 0;
    //int nWritten=0;
    uint64_t numRings = varUintFromBytes(geoPtr, &nRead);
    geoPtr+= nRead;
    newGeoPtr += varUintToBytes(numRings, newGeoPtr);
            
    while (numRings--)
    {
        
        uint64_t numPoints = varUintFromBytes(geoPtr, &nRead);
        geoPtr += nRead;
        newGeoPtr += varUintToBytes(numPoints, newGeoPtr);
        
        assert(numPoints < 10000000 && "overflow"); //sanity check, not an actual limit
        
        int64_t xIn = 0;
        int64_t yIn = 0;
        int64_t prevXOut = 0;
        int64_t prevYOut = 0;
        while (numPoints-- > 0)
        {
            xIn += varIntFromBytes(geoPtr, &nRead); //resolve delta encoding
            geoPtr += nRead;
            yIn += varIntFromBytes(geoPtr, &nRead);
            geoPtr += nRead;
            
            MUST( xIn >= INT32_MIN && xIn <= INT32_MAX, "overflow");
            MUST( yIn >= INT32_MIN && yIn <= INT32_MAX, "overflow");
            
            int32_t xOut = xIn;
            int32_t yOut = yIn;
            convertWgs84ToWebMercator( xOut, yOut);
            
            int64_t dX = xOut - prevXOut;
            int64_t dY = yOut - prevYOut;
            newGeoPtr += varIntToBytes(dX, newGeoPtr);
            newGeoPtr += varIntToBytes(dY, newGeoPtr);

            prevXOut = xOut;
            prevYOut = yOut;   
        }
    }
    uint64_t newSize = newGeoPtr - newData;
    MUST( newSize <= newAllocatedSize, "overflow");
    delete [] geom.bytes;
    geom.bytes = newData;
    geom.numBytes = newSize;
    geom.numBytesAllocated = newAllocatedSize;
}
