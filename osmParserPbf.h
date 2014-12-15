
#ifndef OSM_PARSER_PBF_H
#define OSM_PARSER_PBF_H

#include <vector>

#include "osmConsumer.h"

#include "proto/fileformat.pb.h"
#include "proto/osmformat.pb.h"

class StringTable {
public:
    StringTable(const google::protobuf::RepeatedPtrField<string>& src);
        
    StringTable( const StringTable &other); //not defined to prevent copying
    StringTable& operator=(const StringTable &other); //not defined to prevent copying
        const string& operator[](uint32_t idx) const;
private:
    vector<string> table;
};


class OsmParserPbf
{

public:
    OsmParserPbf(FILE * file, OsmBaseConsumer *consumer);
    ~OsmParserPbf();
    
    void parse();
private:

    void   unpackBlob( const OSMPBF::Blob &blob, FILE* fIn, uint8_t *unpackBufferOut, uint32_t &unpackedSizeOut);
    string prepareBlob(FILE* f, uint8_t *unpackBuffer, uint32_t &dataSizeOut);
    void   parseDenseNodes( const OSMPBF::DenseNodes &nodes, const StringTable &stringTable, int32_t granularity, int64_t lat_offset, int64_t lon_offset);
    void   parseWays(const google::protobuf::RepeatedPtrField<OSMPBF::Way> &ways, const StringTable &stringTable);
    void   parseRelations(const google::protobuf::RepeatedPtrField<OSMPBF::Relation> &rels, const StringTable &stringTable);

/*    void parseNode();    
    void parseWay();    
    void parseRelation();
    void parseChangeset();*/
//    bool readNextLine();
 
private:
    
    FILE * f; 
    OsmBaseConsumer *consumer;
    uint8_t *unpackBuffer;
//    char* line_buffer;
//    size_t line_buffer_size;
//    uint64_t numLinesRead;    
//    const char* line;
    bool hasParsedNodes, hasParsedWays, hasParsedRelations;
};

#endif
