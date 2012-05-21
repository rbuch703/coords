
#include "geometric_types.h"


std::ostream& operator <<(std::ostream& os, const Vertex v)
{
    os << "( " << v.x << ", " << v.y << ")";
    return os;
}


std::ostream& operator <<(std::ostream& os, const PolygonSegment &seg)
{
    for (list<Vertex>::const_iterator it = seg.vertices().begin(); it != seg.vertices().end(); it++)
        os << *it << endl;
    return os;
}


