

#include <iostream>

#include "osmMappedTypes.h"

using namespace std;

class GeoAABB{
public:
    GeoAABB( int32_t lat, int32_t lng): latMin(lat), latMax(lat), lngMin(lng), lngMax(lng) {};
    void add (int32_t lat, int32_t lng)
    {
        if (lat > latMax) latMax = lat;
        if (lat < latMin) latMin = lat;
        if (lng > lngMax) lngMax = lng;
        if (lng < lngMin) lngMin = lng;
    }
    
    int32_t latMin, latMax;
    int32_t lngMin, lngMax;
};

ostream& operator<<(ostream &os, const GeoAABB &aabb)
{
    os << "( lat: " << aabb.latMin << " -> " << aabb.latMax 
       << "; lng: " << aabb.lngMin << " -> " << aabb.lngMax << ")" << endl;
         
    return os;
} 

int main()
{
    LightweightWayStore wayStore("intermediate/ways.idx", "intermediate/ways.data");

    uint64_t numHighways = 0;
    uint64_t pos = 0;
    for (OsmLightweightWay way: wayStore)
    {
        pos += 1;
        if (pos % 1000000 == 0)
            cout << (pos / 1000000) << "M ways read" << endl;
        
        if (!way.hasKey("highway"))
            continue;
        
        GeoAABB aabb( way.vertices[0].lat, way.vertices[0].lng);
        
        for (const OsmGeoPosition &pos : way.getVertices())
            aabb.add( pos.lat, pos.lng );

        cout << way << " " << aabb << endl; 
        numHighways += 1;
    }
 
    cout << numHighways << " roads" << endl;   
    return EXIT_SUCCESS;
}
