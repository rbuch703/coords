
#ifndef OSM_PARSER_PBF
#define OSM_PARSER_PBF

#include "osmParserInterface.h"

class OsmParserPbf: IOsmParser
{

public:
    OsmParserPbf(FILE * file);
    ~OsmParserPbf();
    
    void parse();
private:

/*    void parseNode();    
    void parseWay();    
    void parseRelation();
    void parseChangeset();*/
//    bool readNextLine();
 
private:
    
    FILE * in_file; 
//    char* line_buffer;
//    size_t line_buffer_size;
//    uint64_t numLinesRead;    
//    const char* line;
    bool hasParsedNodes, hasParsedWays, hasParsedRelations;
};

#endif
