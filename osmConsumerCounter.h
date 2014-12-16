
#ifndef OSM_CONSUMER_COUNTER_H
#define OSM_CONSUMER_COUNTER_H

#include "osmConsumer.h"

class OsmConsumerCounter : public OsmBaseConsumer {
public:
    OsmConsumerCounter();

    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OSMRelation &);
    virtual void onAllRelationsConsumed ();
private:
    uint64_t nNodes, nWays, nRelations;
};

#endif
