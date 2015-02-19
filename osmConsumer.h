
#ifndef OSM_PARSER_CONSUMER_H
#define OSM_PARSER_CONSUMER_H

#include "osmTypes.h"


class OsmBaseConsumer
{
public:
    OsmBaseConsumer();
    virtual ~OsmBaseConsumer();
    virtual void finalize();
    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OsmRelation &);

private:
    bool finalized;
    bool hasConsumedNodes, hasConsumedWays, hasConsumedRelations;
    uint64_t prevNodeId, prevWayId, prevRelationId;
};


#endif

