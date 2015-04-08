
#ifndef OSM_PARSER_CONSUMER_H
#define OSM_PARSER_CONSUMER_H

#include "osm/osmTypes.h"


class OsmBaseConsumer
{
public:
    OsmBaseConsumer();
    virtual ~OsmBaseConsumer();
    virtual void consumeNode    ( OsmNode &);
    virtual void consumeWay     ( OsmWay  &);
    virtual void consumeRelation( OsmRelation &);
};


#endif

