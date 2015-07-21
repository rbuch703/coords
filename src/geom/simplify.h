
#ifndef SIMPLIFY_H
#define SIMPLIFY_H

#include <osm/osmBaseTypes.h>
#include <vector>

void simplifyLine(std::vector<OsmGeoPosition> &vertices, double allowedDeviation);


#endif
