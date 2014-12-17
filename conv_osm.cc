
#include <stdio.h>
#include "osmConsumerCounter.h"
#include "osmConsumerDumper.h"
#include "osmConsumerIdRemapper.h"
#include "osmParserPbf.h"
#include "osmParserXml.h"

#include <iostream>

int main(int argc, char** argv)
{
    assert(sizeof(OsmGeoPosition) == sizeof(uint64_t));
    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " inputfile.pbf" << std::endl;
        exit(EXIT_FAILURE);
    }
    FILE* f = fopen( argv[1], "rb");
    
    OsmConsumerDumper dumper;
    OsmConsumerIdRemapper remapper( &dumper);
    OsmParserPbf parser(f, &remapper);
    parser.parse();
    
    fclose(f);


}
