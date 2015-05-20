
#ifndef GENERIC_GEOMETRY_H
#define GENERIC_GEOMETRY_H

#ifndef COORDS_MAPNIK_PLUGIN
    #include "geom/envelope.h"
    #include "osm/osmBaseTypes.h"
    #include "misc/rawTags.h"
#else
    #include "envelope.h"
    #include "rawTags.h"
    enum struct OSM_ENTITY_TYPE : uint8_t { NODE, WAY, RELATION, CHANGESET, OTHER };
#endif

#include <iostream>
#include <string>
#include <vector>

enum struct FEATURE_TYPE: uint8_t {POINT, LINE, POLYGON};

std::ostream& operator<<(std::ostream& os, FEATURE_TYPE ft);
std::ostream& operator<<(std::ostream& os, OSM_ENTITY_TYPE et);

typedef std::pair<std::string, std::string> Tag;

class GenericGeometry {
public:
    GenericGeometry(FILE* f);
    GenericGeometry(const GenericGeometry &other);
#ifdef COORDS_MAPNIK_PLUGIN
    GenericGeometry();
#endif

    ~GenericGeometry();
    
    void init(FILE *F, bool avoidRealloc);
    
    FEATURE_TYPE getFeatureType() const;    //POINT/LINE/POLYGON
    OSM_ENTITY_TYPE getEntityType() const;  //NODE/WAY/RELATION
    uint64_t getEntityId() const;
    Envelope getBounds() const;    
    //std::vector<Tag> getTags() const;
    RawTags getTags() const;
    const uint8_t* getGeometryPtr() const;
private:
    Envelope getLineBounds() const;
    Envelope getPolygonBounds() const;
public:
    uint32_t numBytes;
    uint32_t numBytesAllocated;
    uint8_t *bytes;
    
        
};

#endif

