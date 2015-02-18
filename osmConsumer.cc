
#include "osmConsumer.h"

#include <iostream>


OsmBaseConsumer::OsmBaseConsumer(): 
        finalized(false), hasConsumedNodes(false), 
        hasConsumedWays(false), hasConsumedRelations(false),
        prevNodeId(0), prevWayId(0), prevRelationId(0) {}

OsmBaseConsumer::~OsmBaseConsumer()
{
    if (!finalized)
        finalize();
}

void OsmBaseConsumer::processNode( OSMNode &node) {
    MUST(!hasConsumedWays && !hasConsumedRelations, "order mismatch: node after way or relation");
    MUST(node.id > prevNodeId, "node order inversion");
    this->consumeNode(node);
    hasConsumedNodes = true;   
    prevNodeId = node.id;   
}

void OsmBaseConsumer::processWay( OSMWay &way) {
    MUST(!hasConsumedRelations, "order mismatch: way after relation");
    MUST(way.id > prevWayId, "way order inversion"); 
    if (!hasConsumedWays)
        this->onAllNodesConsumed();
    
    this->consumeWay(way);
    hasConsumedWays = true;
    prevWayId = way.id;
}

void OsmBaseConsumer::processRelation( OsmRelation &relation)
{
    MUST(relation.id > prevRelationId, "relation order inversion"); 
    /* Normally, the first call to processWay() triggers the execution of
     * onFinalNodeConsumed(). But if no way was present in the input data,
     * that never happened. The next if block makes up for that case
    */
    if (!hasConsumedWays)
    {
        this->onAllNodesConsumed();
        hasConsumedWays = true;
    }
    
    if (!hasConsumedRelations)
        this->onAllWaysConsumed();
    
    this->consumeRelation(relation);
    hasConsumedRelations = true;
    prevRelationId = relation.id;
}


//stubs, to be overridden by a derived class
void OsmBaseConsumer::consumeNode    ( OSMNode &) {};
void OsmBaseConsumer::consumeWay     ( OSMWay  &) {};
void OsmBaseConsumer::consumeRelation( OsmRelation &) {};
void OsmBaseConsumer::onAllNodesConsumed () {};  
void OsmBaseConsumer::onAllWaysConsumed () {};   
void OsmBaseConsumer::onAllRelationsConsumed () {}; 

void OsmBaseConsumer::finalize() { 
    /* This method ensures that all of the onAll*Consumed() methods are 
     * called at least once, even if no entity of a given type (e.g. no 
     * way) was present in the input data. (It is also the only facility
     * at all to call onAllRelationsConsumed()
    */
    if (!hasConsumedNodes)
        this->onAllNodesConsumed();
    hasConsumedNodes = true;
    
    if (!hasConsumedWays)
        this->onAllWaysConsumed();
    hasConsumedWays = true;
    
    this->onAllRelationsConsumed();
    hasConsumedRelations = true;
    
    std::cout << "highest IDs: node:" << prevNodeId << ", way:" << prevWayId 
              << ", relation:" << prevRelationId << std::endl;
              
    finalized = true;
}
    

