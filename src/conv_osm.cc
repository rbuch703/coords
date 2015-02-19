
#include <stdio.h>
#include <getopt.h> //for getopt_long()
#include <iostream>

#include "consumers/osmConsumerCounter.h"
#include "consumers/osmConsumerDumper.h"
#include "consumers/osmConsumerIdRemapper.h"
#include "osm/osmParserPbf.h"
//#include "osm/osmParserXml.h"

bool remapIds = 0;

int parseArguments(int argc, char** argv)
{
    static const struct option long_options[] =
    {
        {"remap", no_argument, NULL, 'r'},
        {0,0,0,0}
    };

    int opt_idx = 0;
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "r", long_options, &opt_idx)))
    {
        switch(opt) {
            case '?': exit(EXIT_FAILURE); break; //unknown option; getopt_long() already printed an error message
            case 'r': remapIds = true; break;
            default: abort(); break;
        }
    }
    return optind;
}

int main(int argc, char** argv)
{
    int nextArgumentIndex = parseArguments(argc, argv);
    
    if (nextArgumentIndex == argc)
    {
        std::cerr << "error: missing input file argument" << std::endl;
        std::cerr << "usage: " << argv[0] << " [-r|--remap] <inputfile.pbf>" << std::endl;
        exit(EXIT_FAILURE);
    }

    assert(nextArgumentIndex < argc);
    
    FILE* f = fopen( argv[nextArgumentIndex], "rb");
    if (!f)
    {
        std::cerr << "error: cannot open file '" << argv[nextArgumentIndex] << "'" << endl;
        exit(EXIT_FAILURE);
    }
    OsmBaseConsumer *dumper = new OsmConsumerDumper();
    OsmBaseConsumer* firstConsumer = remapIds ? new OsmConsumerIdRemapper(dumper) : dumper;
    OsmParserPbf parser(f, firstConsumer);
    parser.parse();
    
    fclose(f);
    
    delete dumper;
    if (firstConsumer != dumper)
        delete firstConsumer;

}
