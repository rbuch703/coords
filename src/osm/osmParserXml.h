
#ifndef OSMXMLPARSER_H
#define OSMXMLPARSER_H

#include <stdio.h>

#include "consumers/osmConsumer.h"

class OsmXmlParser
{

public:
    OsmXmlParser(FILE * file, OsmBaseConsumer *consumer);
    ~OsmXmlParser();
    
    void parse();
private:

    void parseNode();    
    void parseWay();    
    void parseRelation();
    void parseChangeset();
    bool readNextLine();
    
private:
    OsmBaseConsumer *consumer;
    FILE * in_file; 
    char* line_buffer;
    size_t line_buffer_size;
    uint64_t numLinesRead;    
    const char* line;
    bool hasParsedNodes, hasParsedWays, hasParsedRelations;
};



#endif

