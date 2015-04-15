
#include "envelope.h"
#include <iostream>

static inline int32_t max(int32_t a, int32_t b) { return a > b ? a : b;}
static inline int32_t min(int32_t a, int32_t b) { return a < b ? a : b;}

Envelope::Envelope( int32_t lat, int32_t lng): latMin(lat), latMax(lat), lngMin(lng), lngMax(lng) {};

Envelope::Envelope( int32_t latMin, int32_t latMax, int32_t lngMin, int32_t lngMax): 
    latMin(latMin), latMax(latMax), lngMin(lngMin), lngMax(lngMax) {};


void Envelope::add (int32_t lat, int32_t lng)
{
    if (lat > latMax) latMax = lat;
    if (lat < latMin) latMin = lat;
    if (lng > lngMax) lngMax = lng;
    if (lng < lngMin) lngMin = lng;
}


bool Envelope::overlapsWith(const Envelope &other) const
{    
    return (max( lngMin, other.lngMin) <= min( lngMax, other.lngMax)) && 
           (max( latMin, other.latMin) <= min( latMax, other.latMax));
}

const Envelope& Envelope::getWorldBounds() 
{ 
    static Envelope world = (Envelope){
        .latMin = -900000000, .latMax = 900000000, 
        .lngMin = -1800000000,.lngMin = 1800000000};
    return world;
}

Envelope getBounds(const OsmLightweightWay &way)
{
    Envelope aabb( way.vertices[0].lat, way.vertices[0].lng);
        
    for (const OsmGeoPosition &pos : way.getVertices())
        aabb.add( pos.lat, pos.lng );

    return aabb;
}

std::ostream& operator<<(std::ostream &os, const Envelope &aabb)
{
    os << "( lat: " << aabb.latMin << " -> " << aabb.latMax 
       << "; lng: " << aabb.lngMin << " -> " << aabb.lngMax << ")";
         
    return os;
}

