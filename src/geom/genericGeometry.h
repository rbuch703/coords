
#ifndef GENERIC_GEOMETRY_H
#define GENERIC_GEOMETRY_H

#include "config.h"
#include "geom/envelope.h"
#include "osm/osmBaseTypes.h"

enum struct FEATURE_TYPE: uint8_t {POINT, LINE, POLYGON};
std::ostream& operator<<(std::ostream& os, FEATURE_TYPE ft);
std::ostream& operator<<(std::ostream& os, OSM_ENTITY_TYPE et);



class OpaqueOnDiskGeometry {
public:
    OpaqueOnDiskGeometry(FILE* f);
    ~OpaqueOnDiskGeometry();
    
    FEATURE_TYPE getFeatureType() const;    //POINT/LINE/POLYGON
    OSM_ENTITY_TYPE getEntityType() const;  //NODE/WAY/RELATION
    uint64_t getEntityId() const;
    Envelope getBounds() const;    
private:
    Envelope getLineBounds() const;
    Envelope getPolygonBounds() const;
public:
    uint32_t numBytes;
    uint8_t *bytes;
    
        
};

#endif

