
#include <stdio.h>
#include <getopt.h> //for getopt_long()
#include <unistd.h> //for unlink()
#include <sys/stat.h>
#include <google/protobuf/stubs/common.h>   //for ShutdownProtobufLibrary()
#include <iostream>

#include "consumers/osmConsumerCounter.h"
#include "consumers/osmConsumerDumper.h"
#include "consumers/osmConsumerIdRemapper.h"
#include "osm/osmParserPbf.h"
//#include "osm/osmParserXml.h"

bool remapIds = 0;
std::string destinationDirectory;

int parseArguments(int argc, char** argv)
{
    static const struct option long_options[] =
    {
        {"remap", no_argument,       NULL, 'r'},
        {"dest",  required_argument, NULL, 'd'},
        {0,0,0,0}
    };

    int opt_idx = 0;
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "rd:", long_options, &opt_idx)))
    {
        switch(opt) {
            case '?': exit(EXIT_FAILURE); break; //unknown option; getopt_long() already printed an error message
            case 'r': remapIds = true; break;
            case 'd': destinationDirectory = optarg; break;
            default: abort(); break;
        }
    }
    return optind;
}

void deleteFileIfExists(string filename)
{
    int res = unlink(filename.c_str());
    MUST(res == 0 || errno == ENOENT, "cannot access file");
}

int main(int argc, char** argv)
{
    int nextArgumentIndex = parseArguments(argc, argv);
    std::string usageLine = std::string("usage: ") + argv[0] + " [-r|--remap] --dest <destination directory> <inputfile.pbf>";
    if (nextArgumentIndex == argc)
    {
        std::cerr << "error: missing input file argument" << std::endl;
        std::cerr << usageLine << std::endl;
        exit(EXIT_FAILURE);
    }
    
    if (destinationDirectory == "")
    {
        std::cerr << "error: missing required argument '--dest'" << endl;
        std::cerr << usageLine << std::endl;
        exit(EXIT_FAILURE);
    }
    
    struct stat st;
    // note: stat follows symlinks, so it will never stat() the symlink itself.
    int res = stat( destinationDirectory.c_str(), &st);
    if (res != 0)
    {
        switch (errno)
        {
            case EACCES: std::cerr << "error: insufficient permissions to access destination '" + destinationDirectory << "'." << endl; break;
            case ENOENT: std::cerr << "error: destination directory '" << destinationDirectory << "' does not exist." << endl; break;
            default:     std::cerr << "error: cannot access destination directory'" << destinationDirectory << "'." << endl; break;
        }
        exit(EXIT_FAILURE);
    }
    
    if (! S_ISDIR(st.st_mode) && ! S_ISLNK(st.st_mode))
    {
        std::cerr << "error: destination '" << destinationDirectory << "' is not a (symlink to a) directory." << std::endl;
        exit(0);
    }
    
    MUST(destinationDirectory.length() > 0, "empty destination given");
    if (destinationDirectory.back() != '/' && destinationDirectory.back() != '\\')
        destinationDirectory += "/";
    
    //remove leftover files from earlier runs:
    // - remap files
    deleteFileIfExists(destinationDirectory + "mapNodes.idx"    );
    deleteFileIfExists(destinationDirectory + "mapRelations.idx");
    deleteFileIfExists(destinationDirectory + "mapWays.idx"     );

    // - reverse indices
    deleteFileIfExists(destinationDirectory + "nodeReverse.idx"    );
    deleteFileIfExists(destinationDirectory + "nodeReverse.aux"    );
    deleteFileIfExists(destinationDirectory + "wayReverse.idx"     );
    deleteFileIfExists(destinationDirectory + "wayReverse.aux"     );
    deleteFileIfExists(destinationDirectory + "relationReverse.idx");
    deleteFileIfExists(destinationDirectory + "relationReverse.aux");

    

    MUST(nextArgumentIndex < argc, "argv index out of bounds");
    
    FILE* f = fopen( argv[nextArgumentIndex], "rb");
    if (!f)
    {
        std::cerr << "error: cannot open file '" << argv[nextArgumentIndex] << "'" << endl;
        exit(EXIT_FAILURE);
    }

    OsmBaseConsumer *dumper = new OsmConsumerDumper(destinationDirectory);
    OsmBaseConsumer* firstConsumer = remapIds ? 
        new OsmConsumerIdRemapper(destinationDirectory, dumper) : dumper;
    OsmParserPbf parser(f, firstConsumer);
    parser.parse();
    
    fclose(f);
    
    delete dumper;
    if (firstConsumer != dumper)
        delete firstConsumer;

    google::protobuf::ShutdownProtobufLibrary();
}
