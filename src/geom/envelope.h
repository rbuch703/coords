
#ifndef GEOM_ENVELOPE_H
#define GEOM_ENVELOPE_H

#include <stdint.h>
#include <iostream>
//#include "osm/osmMappedTypes.h"


class Envelope{
public:
    Envelope();
    Envelope( int32_t x, int32_t y);
    Envelope( int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax);

    void add (int32_t x, int32_t y);
    bool overlapsWith(const Envelope &other) const;
    bool contains(const Envelope &other) const;
    bool isValid() const;    
public:
    int32_t xMin, xMax;
    int32_t yMin, yMax;
    
};

std::ostream& operator<<(std::ostream &os, const Envelope &aabb);


#endif

