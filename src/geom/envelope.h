
#ifndef GEOM_ENVELOPE_H
#define GEOM_ENVELOPE_H

#include <stdint.h>
#include <iostream>
//#include "osm/osmMappedTypes.h"


class Envelope{
public:
    Envelope();
    Envelope( int32_t lat, int32_t lng);
    Envelope( int32_t latMin, int32_t latMax, int32_t lngMin, int32_t lngMax);

    void add (int32_t lat, int32_t lng);
    bool overlapsWith(const Envelope &other) const;
    bool isValid() const;    
public:
    int32_t latMin, latMax;
    int32_t lngMin, lngMax;
    
};

std::ostream& operator<<(std::ostream &os, const Envelope &aabb);


#endif

