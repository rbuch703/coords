
#ifndef OSM_PARSER_INTERFACE
#define OSM_PARSER_INTERFACE

#include "osm_types.h"

class IOsmParser
{
protected:
    virtual void completedNode    ( OSMNode &) = 0;
    virtual void completedWay     ( OSMWay  &) = 0;
    virtual void completedRelation( OSMRelation &) = 0;

    virtual void beforeParsingNodes () {};  
    virtual void beforeParsingWays () {};   
    virtual void beforeParsingRelations () {}; 

    virtual void afterParsingNodes () {};  
    virtual void afterParsingWays () {};   
    virtual void afterParsingRelations () {}; 
};


#endif

