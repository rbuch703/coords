
#ifndef OSM_CONSUMER_ID_REMAPPER_H
#define OSM_CONSUMER_ID_REMAPPER_H

#include "osmConsumer.h"

class Remap;

class OsmConsumerIdRemapper : public OsmBaseConsumer
{

public:
    OsmConsumerIdRemapper(OsmBaseConsumer *innerConsumer);
    virtual ~OsmConsumerIdRemapper();    
protected:
    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OsmRelation &);
    virtual void onAllNodesConsumed();
    virtual void onAllWaysConsumed();
    virtual void onAllRelationsConsumed();

private:
    OsmBaseConsumer *innerConsumer;
    Remap *nodeRemap, *wayRemap, *relationRemap; 
};
#endif
