
#include "osmConsumer.h"

//a macro that is similar to assert(), but is not deactivated by NDEBUG
#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); exit(EXIT_FAILURE);}}


OsmBaseConsumer::OsmBaseConsumer(): 
        finalized(false), hasConsumedNodes(false), 
        hasConsumedWays(false), hasConsumedRelations(false) {}

OsmBaseConsumer::~OsmBaseConsumer()
{
    if (!finalized)
        finalize();
}

void OsmBaseConsumer::processNode( OSMNode &node) {
    MUST(!hasConsumedWays && !hasConsumedRelations, "order mismatch: node after way or relation");
    this->consumeNode(node);
    hasConsumedNodes = true;      
}

void OsmBaseConsumer::processWay( OSMWay &way) {
    MUST(!hasConsumedRelations, "order mismatch: way after relation");
    if (!hasConsumedWays)
        this->onAllNodesConsumed();
    
    this->consumeWay(way);
    hasConsumedWays = true;
}

void OsmBaseConsumer::processRelation( OSMRelation &relation)
{
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
}


//stubs, to be overridden by a derived class
void OsmBaseConsumer::consumeNode    ( OSMNode &) {};
void OsmBaseConsumer::consumeWay     ( OSMWay  &) {};
void OsmBaseConsumer::consumeRelation( OSMRelation &) {};
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
}
    

