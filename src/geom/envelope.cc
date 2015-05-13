
#include "envelope.h"
//#include "config.h"

static inline int32_t max(int32_t a, int32_t b) { return a > b ? a : b;}
static inline int32_t min(int32_t a, int32_t b) { return a < b ? a : b;}


Envelope::Envelope() : xMin(1), xMax(-1),
                       yMin(1), yMax(-1)
{ }


Envelope::Envelope( int32_t x, int32_t y): xMin(x), xMax(x), yMin(y), yMax(y) {};

Envelope::Envelope( int32_t xMin, int32_t xMax, int32_t yMin, int32_t yMax): 
    xMin(xMin), xMax(xMax), yMin(yMin), yMax(yMax) {};


void Envelope::add (int32_t x, int32_t y)
{
    if (xMin > xMax || yMin > yMax) //invalid --> not yet initialized
    {
        xMax = xMin = x;
        yMax = yMin = y;
    }
    if (x > xMax) xMax = x;
    if (x < xMin) xMin = x;
    if (y > yMax) yMax = y;
    if (y < yMin) yMin = y;
}


bool Envelope::overlapsWith(const Envelope &other) const
{   
    if (xMin > xMax || yMin > yMax) //invalid --> not yet initialized
        return false;

    return (max( yMin, other.yMin) <= min( yMax, other.yMax)) && 
           (max( xMin, other.xMin) <= min( xMax, other.xMax));
}

bool Envelope::isValid() const
{
    return (xMin <= xMax && yMin <= yMax);
}

std::ostream& operator<<(std::ostream &os, const Envelope &aabb)
{
    os << "( x: " << aabb.xMin << " -> " << aabb.xMax 
       << "; y: " << aabb.yMin << " -> " << aabb.yMax << ")";
         
    return os;
}

