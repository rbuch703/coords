
#ifndef OSM_PARSER_CONSUMER_H
#define OSM_PARSER_CONSUMER_H

#include "osmTypes.h"


class OsmBaseConsumer
{
public:
    OsmBaseConsumer();
    virtual ~OsmBaseConsumer();
    void processNode( OSMNode &node);
    void processWay( OSMWay &way);
    void processRelation( OSMRelation &relation);
    void finalize();

//only these members are to be overridden to specialize this class
public:
    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OSMRelation &);

        /* The onAll*Consumed() methods are guaranteed 
           - to be called exactly once during parsing,
           - to be called in order nodes->ways->relations
           - to be called at the correct time (e.g. onAllWaysConsumed when no 
             more ways are to be consumed and before the first relation is consumed)
         */

    virtual void onAllNodesConsumed ();
    virtual void onAllWaysConsumed (); 
    virtual void onAllRelationsConsumed(); 

private:
    bool finalized;
    bool hasConsumedNodes, hasConsumedWays, hasConsumedRelations;
    uint64_t prevNodeId, prevWayId, prevRelationId;
};


#endif

