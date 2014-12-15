
#ifndef OSM_PARSER_CONSUMER_H
#define OSM_PARSER_CONSUMER_H

#include "osm_types.h"

//a macro that is similar to assert(), but is not deactivated by NDEBUG
#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); exit(EXIT_FAILURE);}}

class OsmBaseConsumer
{
public:
    OsmBaseConsumer(): 
        finalized(false), hasConsumedNodes(false), 
        hasConsumedWays(false), hasConsumedRelations(false) {}

    virtual ~OsmBaseConsumer()
    {
        if (!finalized)
            finalize();
    }
    
    void processNode( OSMNode &node) {
        MUST(!hasConsumedWays && !hasConsumedRelations, "order mismatch: node after way or relation");
        this->consumeNode(node);
        hasConsumedNodes = true;      
    }
    
    void processWay( OSMWay &way) {
        MUST(!hasConsumedRelations, "order mismatch: way after relation");
        if (!hasConsumedWays)
            this->onAllNodesConsumed();
        
        this->consumeWay(way);
        hasConsumedWays = true;
    }
    
    void processRelation( OSMRelation &relation)
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


protected:
    virtual void consumeNode    ( OSMNode &) {};
    virtual void consumeWay     ( OSMWay  &) {};
    virtual void consumeRelation( OSMRelation &) {};

        /* The onAll*Consumed() methods are guaranteed 
           - to be called exactly once during parsing,
           - to be called in order nodes->ways->relations
           - to be called at the correct time (e.g. onAllWaysConsumed when no 
             more ways are to be consumed and before the first relation is consumed)
         */

    virtual void onAllNodesConsumed () {};  
    virtual void onAllWaysConsumed () {};   
    virtual void onAllRelationsConsumed () {}; 

public:
    void finalize() { 
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
    
private:
    bool finalized;
    bool hasConsumedNodes, hasConsumedWays, hasConsumedRelations;

};


#endif

