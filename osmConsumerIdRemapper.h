
#ifndef OSM_CONSUMER_ID_REMAPPER_H
#define OSM_CONSUMER_ID_REMAPPER_H

#include "osmConsumer.h"

typedef pair<uint64_t, int64_t> RemapEntry;


class OsmConsumerIdRemapper : public OsmBaseConsumer
{

public:
    OsmConsumerIdRemapper(OsmBaseConsumer *innerConsumer);
    virtual ~OsmConsumerIdRemapper();    
protected:
    RemapEntry getRemapEntry(uint64_t nodeId);
    void appendRemapEntry(uint64_t nodeId, int64_t offset);

    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OSMRelation &);
    virtual void onAllNodesConsumed();
    virtual void onAllWaysConsumed();
    virtual void onAllRelationsConsumed();

private:
    OsmBaseConsumer *innerConsumer;
    RemapEntry *remap;
    int numRemapEntries;
    int maxNumRemapEntries;
    uint64_t nextFreeId;
    uint64_t nextNodeId; 
};
#endif
