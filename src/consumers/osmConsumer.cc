
#include "consumers/osmConsumer.h"

#include <iostream>


OsmBaseConsumer::OsmBaseConsumer() {}
OsmBaseConsumer::~OsmBaseConsumer(){}

//stubs, to be overridden by a derived class
void OsmBaseConsumer::consumeNode    ( OsmNode &) {};
void OsmBaseConsumer::consumeWay     ( OsmWay  &) {};
void OsmBaseConsumer::consumeRelation( OsmRelation &) {};
    

