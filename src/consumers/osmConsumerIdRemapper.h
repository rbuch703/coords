
#ifndef OSM_CONSUMER_ID_REMAPPER_H
#define OSM_CONSUMER_ID_REMAPPER_H

#include "consumers/osmConsumer.h"
#include "containers/serializableMap.h"
//class Remap;

class OsmConsumerIdRemapper : public OsmBaseConsumer
{

public:
    OsmConsumerIdRemapper(OsmBaseConsumer *innerConsumer);
    virtual ~OsmConsumerIdRemapper();    
protected:
    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OsmRelation &);
private:
    OsmBaseConsumer *innerConsumer;
    SerializableMap<uint64_t, uint64_t, 10000000> nodeMap, wayMap, relationMap;
    uint64_t nodeIdsRemapped, wayIdsRemapped, relationIdsRemapped;
//    Remap *nodeRemap, *wayRemap, *relationRemap; 
};
#endif
