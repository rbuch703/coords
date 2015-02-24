
#ifndef OSM_PARSER_CONSUMER_H
#define OSM_PARSER_CONSUMER_H

#include "osm/osmTypes.h"


class OsmBaseConsumer
{
public:
    OsmBaseConsumer();
    virtual ~OsmBaseConsumer();
    virtual void consumeNode    ( OSMNode &);
    virtual void consumeWay     ( OSMWay  &);
    virtual void consumeRelation( OsmRelation &);
};


#endif

