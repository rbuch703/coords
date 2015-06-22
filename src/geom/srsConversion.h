
#ifndef SRS_CONVERSION_H
#define SRS_CONVERSION_H

#include "osm/osmTypes.h"
#include "geom/genericGeometry.h"

void convertWgs84ToWebMercator( OsmWay &way);
void convertWgs84ToWebMercator( GenericGeometry &geom);
void convertWgs84ToWebMercator( int32_t &lat, int32_t &lng);


#endif

