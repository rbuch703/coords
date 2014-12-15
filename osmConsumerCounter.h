
#ifndef OSM_CONSUMER_COUNTER_H
#define OSM_CONSUMER_COUNTER_H

#include "osmConsumer.h"

class OsmConsumerCounter : public OsmBaseConsumer {
public:
    OsmConsumerCounter(): nNodes(0), nWays(0), nRelations(0) {}

    virtual void consumeNode    ( OSMNode &)     { nNodes     += 1;}
    virtual void consumeWay     ( OSMWay  &)     { nWays      += 1;}
    virtual void consumeRelation( OSMRelation &) { nRelations += 1;}

    virtual void onAllRelationsConsumed () 
    {
        cout << "stats: " << nNodes << "/" << nWays << "/" << nRelations << endl; 
    }
    
private:
    uint64_t nNodes, nWays, nRelations;

};

#endif
