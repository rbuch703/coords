
#include "osmConsumer.h"

#include <iostream>


OsmBaseConsumer::OsmBaseConsumer(): 
        finalized(false) {}

OsmBaseConsumer::~OsmBaseConsumer()
{
}

//stubs, to be overridden by a derived class
void OsmBaseConsumer::consumeNode    ( OSMNode &) {};
void OsmBaseConsumer::consumeWay     ( OSMWay  &) {};
void OsmBaseConsumer::consumeRelation( OsmRelation &) {};

void OsmBaseConsumer::finalize() { 
}
    

