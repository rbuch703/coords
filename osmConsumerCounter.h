
#ifndef OSM_CONSUMER_COUNTER_H
#define OSM_CONSUMER_COUNTER_H

#include "osmConsumer.h"

class OsmConsumerCounter : public OsmBaseConsumer {
public:
    OsmConsumerCounter();
    virtual ~OsmConsumerCounter();

    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OsmRelation &);
    
private:
    uint64_t nNodes, nWays, nRelations;
};

#endif
