
#ifndef GEOM_SERIALIZATION
#define GEOM_SERIALIZATION

#include "osm/osmTypes.h"
#include "geom/ring.h"
#include "geom/genericGeometry.h"
#include "misc/rawTags.h"

//#include <geos/geom/Geometry.h>

#include <string>
#include <map>

//forward declaration
namespace geos { namespace geom { class Geometry; } }

void serializePolygon(const Ring &poly, const Tags &tags, uint64_t relId, FILE* fOut);

GenericGeometry serializeWay(const OsmWay &way, bool asPolygon);
GenericGeometry serializeNode(const OsmNode &node);


GenericGeometry serialize(geos::geom::Geometry* geom, uint64_t id, const RawTags &tags);

geos::geom::Geometry* createGeosGeometry( const GenericGeometry   &geom);
geos::geom::Geometry* createGeosGeometry( const OsmWay &geom);

/* ON-DISK LAYOUT FOR GEOMETRY
:
    uint32_t size in bytes (not counting the size field itself)
    uint8_t  type ( 0 --> POINT, 1 --> LINE, 2 --> POLYGON)
    uint64_t id ( POINT--> nodeId; LINE--> wayId;
                  POLYGON:  MSB set --> wayId; MSB unset --> relationId)
    <tags>
    <type-specific data>:
    
   1. type == POINT
        int32_t lat;
        int32_t lng;
        
   2. type == LINE
        varUint numPoints;
        'numPoints' times:
            varInt lat; //delta-encoded
            varInt lng; //delta-encoded
    
   3. type == POLYGON
        varUint numRings;
        'numRings' times:
            varUint numPoints;
            'numPoints' times:
                varInt lat; //delta-encoded
                varInt lng; //delta-encoded
    
*/

#endif


