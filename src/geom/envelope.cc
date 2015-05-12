
#include "envelope.h"
//#include "config.h"

static inline int32_t max(int32_t a, int32_t b) { return a > b ? a : b;}
static inline int32_t min(int32_t a, int32_t b) { return a < b ? a : b;}


Envelope::Envelope() : latMin(1), latMax(-1),
                       lngMin(1), lngMax(-1)
{ }


Envelope::Envelope( int32_t lat, int32_t lng): latMin(lat), latMax(lat), lngMin(lng), lngMax(lng) {};

Envelope::Envelope( int32_t latMin, int32_t latMax, int32_t lngMin, int32_t lngMax): 
    latMin(latMin), latMax(latMax), lngMin(lngMin), lngMax(lngMax) {};


void Envelope::add (int32_t lat, int32_t lng)
{
    if (latMin > latMax || lngMin > lngMax) //invalid --> not yet initialized
    {
        latMax = latMin = lat;
        lngMax = lngMin = lng;
    }
    if (lat > latMax) latMax = lat;
    if (lat < latMin) latMin = lat;
    if (lng > lngMax) lngMax = lng;
    if (lng < lngMin) lngMin = lng;
}


bool Envelope::overlapsWith(const Envelope &other) const
{   
    if (latMin > latMax || lngMin > lngMax) //invalid --> not yet initialized
        return false;

    return (max( lngMin, other.lngMin) <= min( lngMax, other.lngMax)) && 
           (max( latMin, other.latMin) <= min( latMax, other.latMax));
}

bool Envelope::isValid() const
{
    return (latMin <= latMax && lngMin <= lngMax);
}

std::ostream& operator<<(std::ostream &os, const Envelope &aabb)
{
    os << "( lat: " << aabb.latMin << " -> " << aabb.latMax 
       << "; lng: " << aabb.lngMin << " -> " << aabb.lngMax << ")";
         
    return os;
}

