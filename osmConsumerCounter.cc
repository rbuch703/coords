
#include "osmConsumerCounter.h"

#include <iostream>

OsmConsumerCounter::OsmConsumerCounter(): nNodes(0), nWays(0), nRelations(0) {}


void OsmConsumerCounter::consumeNode    ( OSMNode &)     { nNodes     += 1;}
void OsmConsumerCounter::consumeWay     ( OSMWay  &)     { nWays      += 1;}
void OsmConsumerCounter::consumeRelation( OSMRelation &) { nRelations += 1;}
void OsmConsumerCounter::onAllRelationsConsumed () 
{
    std::cout << "stats: " << nNodes << "/" << nWays << "/" << nRelations << std::endl;
}

