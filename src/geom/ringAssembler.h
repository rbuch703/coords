
#ifndef RING_ASSEMBLER_H
#define RING_ASSEMBLER_H

#include <list>
#include <vector>
#include <map>
#include <string>

#include "osm/osmMappedTypes.h"
#include "geom/ringSegment.h"

class RingAssembler {

    
public:

    void addWay( const OsmLightweightWay &way);
    void warnUnconnectedNodes(uint64_t relId) const;
    void tryCloseOpenSegments( double maxConnectionDistance);
    const std::vector<RingSegment*> &getClosedRings() const;
    
    bool hasOpenRingSegments() const;
    double getAABBDiameter( std::map<uint64_t, OsmLightweightWay> wayStore) const;
    
    static RingAssembler fromRelation( OsmRelation &rel, 
            const std::map<uint64_t, OsmLightweightWay> &ways, 
            std::map<std::string, std::string> &outerTagsOut);
    
private:
    /* cannot be a std::vector, because we use pointers into this container,
     * and pointers into a vector are not guaranteed to be stable when adding
     * or removing elements from/to it (causing a resize of the underlying array) */
    std::list<RingSegment> ringSegments;
    std::vector<RingSegment*> closedRings;
    std::map<OsmGeoPosition, RingSegment*> openEndPoints;


};

#endif

