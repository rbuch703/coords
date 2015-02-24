
#include "consumers/osmConsumer.h"

#include <iostream>


OsmBaseConsumer::OsmBaseConsumer() {}
OsmBaseConsumer::~OsmBaseConsumer(){}

//stubs, to be overridden by a derived class
void OsmBaseConsumer::consumeNode    ( OSMNode &) {};
void OsmBaseConsumer::consumeWay     ( OSMWay  &) {};
void OsmBaseConsumer::consumeRelation( OsmRelation &) {};
    

