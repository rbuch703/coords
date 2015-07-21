
#ifndef GEOM_SERIALIZATION
#define GEOM_SERIALIZATION

#include "osm/osmTypes.h"
#include "geom/ring.h"
#include "geom/genericGeometry.h"
#include "misc/rawTags.h"

//#include <geos/geom/Geometry.h>

#include <string>
#include <map>
#include <vector>

//forward declaration
namespace geos { namespace geom { class Geometry; } }

void serializePolygon(const Ring &poly, const Tags &tags, uint64_t relId, int8_t zIndex, FILE* fOut);

GenericGeometry serializeWay(const OsmWay &way, bool asPolygon, int8_t zIndex);
GenericGeometry serializeWay(uint64_t wayId, const std::vector<OsmGeoPosition> &vertices, const uint8_t *tagBytes, uint64_t numTagBytes, bool asPolygon, int8_t zIndex);
GenericGeometry serializeNode(const OsmNode &node, int8_t zIndex);


GenericGeometry serialize(geos::geom::Geometry* geom, uint64_t id, GEOMETRY_FLAGS flags, int8_t zIndex, const RawTags &tags);

geos::geom::Geometry* createGeosGeometry( const GenericGeometry   &geom);
geos::geom::Geometry* createGeosGeometry( const OsmWay &geom, bool asPolygon );

/* ON-DISK LAYOUT FOR GEOMETRY
:
    uint32_t size in bytes (not counting the size field itself)
    uint8_t  geometryFlags
    int8_t   zIndex
    varUint  id (POINT--> nodeId; LINE/WAY_POLYGON--> wayId; RELATION_POLYGON --> relationId)
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


