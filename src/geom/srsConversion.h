
#ifndef SRS_CONVERSION_H
#define SRS_CONVERSION_H

#include "osm/osmMappedTypes.h"
#include "geom/genericGeometry.h"

void convertWgs84ToWebMercator( OsmLightweightWay &way);
void convertWgs84ToWebMercator( GenericGeometry &geom);


#endif

