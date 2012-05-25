
#ifndef OSMXMLPARSER_H
#define OSMXMLPARSER_H

#include <stdio.h>

#include "osm_types.h"


class OsmXmlParser
{

public:
    OsmXmlParser(FILE * file);
    ~OsmXmlParser();
    
    void parse();
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

private:

    void parseNode();    
    void parseWay();    
    void parseRelation();
    void parseChangeset();
    bool readNextLine();
    
private:
    
    FILE * in_file; 
    char* line_buffer;
    size_t line_buffer_size;
    uint64_t numLinesRead;    
    const char* line;
    bool hasParsedNodes, hasParsedWays, hasParsedRelations;
};



#endif

