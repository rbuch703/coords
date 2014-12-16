
#include <stdio.h>
#include "osmConsumerCounter.h"
#include "osmConsumerDumper.h"
#include "osmParserPbf.h"
#include "osmParserXml.h"

#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "usage: " << argv[0] << " inputfile.pbf" << std::endl;
        exit(EXIT_FAILURE);
    }

    FILE* f = fopen( argv[1], "rb");
    
    OsmConsumerCounter counter;
    OsmParserPbf parser(f, &counter);
    parser.parse();
    
    fclose(f);


}
