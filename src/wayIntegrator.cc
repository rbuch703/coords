
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <unistd.h> //for sysconf()
#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h> //for stat()
#include <sys/resource.h>   //for getrlimit()


#include "mem_map.h"
#include "osm/osmMappedTypes.h"
#include "containers/reverseIndex.h"


using namespace std;


bool modeIsRandomAccess = true;
bool dryRun = false;
bool overrideRunLimit = false;
uint64_t maxMmapSize = 0;
std::string storageDirectory;
std::string usageLine;

int parseArguments(int argc, char** argv)
{
    static const struct option long_options[] =
    {
        {"mode",    required_argument,  NULL, 'm'},
        {"lock",    required_argument,  NULL, 'l'},
        {"yes",     no_argument,        NULL, 'y'},
        {"dryrun",  no_argument,        NULL, 'd'},
        {0,0,0,0}
    };

    int opt_idx = 0;
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "m:l:yd", long_options, &opt_idx)))
    {
        switch(opt) {
            case '?': exit(EXIT_FAILURE); break; //unknown option; getopt_long() already printed an error message
            case 'm':
                if (string(optarg) == "BLOCK") modeIsRandomAccess = false;
                else if (string(optarg) == "RANDOM") {} //do nothing, is default anyway
                else
                {
                    std::cerr << "error: invalid mode '" << optarg << "'" << endl;
                    std::cerr << usageLine << std::endl;
                    exit(EXIT_FAILURE);
                }
                break;
            case 'l':
                char* endPtr;
                maxMmapSize = strtoll(optarg, &endPtr, 10);
                if ( (*endPtr != '\0') || (maxMmapSize == 0) )
                {
                    std::cerr << "error: invalid lock size '" << optarg << "'" << endl;
                    exit(EXIT_FAILURE);
                }
                maxMmapSize *= 1000000; //was given in MB
                break;
            case 'y':
                overrideRunLimit = true;
                break;
            case 'd':
                dryRun = true;
                break;
            default: abort(); break;
        }
    }
    
    if (optind == argc)
    {
        std::cerr << "error: missing input file argument" << std::endl;
        std::cerr << usageLine << std::endl;
        exit(EXIT_FAILURE);
    }
    MUST(optind < argc, "argv index out of bounds");
    
    storageDirectory = argv[optind++];
    
    return optind;
}


uint64_t getRunConfiguration(FILE* vertexIndexFile, uint64_t maxMmapSize, 
                        uint64_t *vertexWindowSize, uint64_t *nVerticesPerRun)
{
    fseek( vertexIndexFile, 0, SEEK_END);
    uint64_t numVertices = ftell(vertexIndexFile) / (2 * sizeof(int32_t));
    MUST(numVertices > 0, "empty vertex map");

    uint64_t pageSize = sysconf(_SC_PAGE_SIZE);
    uint64_t maxNumConcurrentPages = maxMmapSize / pageSize;
    //the remaining code assumes that no vertex is spread across more than one page
    assert( pageSize % (2*sizeof(int32_t)) == 0); 
    uint64_t nVerticesPerPage = pageSize / (2*sizeof(int32_t));

    uint64_t numVertexBytes = numVertices * (2*sizeof(int32_t));
    uint64_t numVertexPages = (numVertexBytes+ (pageSize-1) ) / pageSize;  //rounded up
    cout << "[INFO] Index contains " << (numVertices/1000000) << "M nodes (" 
         << (numVertexBytes/1000000) << "MB)" << endl;

    uint64_t nRuns = ( numVertexPages + (maxNumConcurrentPages - 1) ) / (maxNumConcurrentPages); //rounding up!
    assert( nRuns * maxMmapSize >= numVertexBytes);
    uint64_t nPagesPerRun = (numVertexPages + (nRuns -1)) / nRuns;  //rounded up to cover all vertices!
    *nVerticesPerRun =  nPagesPerRun * nVerticesPerPage;
    assert( nRuns * (*nVerticesPerRun) >= numVertices);

    *vertexWindowSize = (*nVerticesPerRun) * (2 * sizeof(int32_t));
    return nRuns;
}


void ensureMlockSize(uint64_t maxMmapSize)
{
   assert(maxMmapSize % sysconf(_SC_PAGE_SIZE) == 0 && "invalid mmap size");
   
    struct rlimit limit;
    getrlimit(RLIMIT_MEMLOCK, &limit);
    
    if (limit.rlim_max < maxMmapSize)
        limit.rlim_max = maxMmapSize;
        
    if (limit.rlim_cur < maxMmapSize)
        limit.rlim_cur = maxMmapSize;
    
    int res = setrlimit(RLIMIT_MEMLOCK, &limit);
    if (res != 0)
    {
        if (errno == EPERM)
        {
            cout << "error: current user does not have permission to mlock() the selected amount of memory." << endl;
            cout << "       Raise the corresponding ulimit or run as root." << endl;
        }
        else
            perror("getrlimit error:");
        exit(EXIT_FAILURE);
    }

}

int main(int argc, char** argv)
{

    usageLine = std::string() + "usage: " + argv[0] + " --mode [RANDOM | BLOCK --lock <size in MB>] (--dryrun) (--yes) <storage directory>";
    parseArguments(argc, argv);

    MUST(storageDirectory != "", "Command line parameter parse error");
    
    struct stat st;
    // note: stat follows symlinks, so it will never stat() the symlink itself.
    int res = stat( storageDirectory.c_str(), &st);
    if (res != 0)
    {
        switch (errno)
        {
            case EACCES: std::cerr << "error: insufficient permissions to access storage directory '" + storageDirectory << "'." << endl; break;
            case ENOENT: std::cerr << "error: destination directory '" << storageDirectory << "' does not exist." << endl; break;
            default:     std::cerr << "error: cannot access destination directory'" << storageDirectory << "'." << endl; break;
        }
        exit(EXIT_FAILURE);
    }
    
    if (! S_ISDIR(st.st_mode) && ! S_ISLNK(st.st_mode))
    {
        std::cerr << "error: destination '" << storageDirectory << "' is not a (symlink to a) directory." << std::endl;
        exit(0);
    }
    
    MUST(storageDirectory.length() > 0, "empty destination given");
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";
    

    //ReverseIndex reverseNodeIndex(storageDirectory + "nodeReverse.idx", storageDirectory +"nodeReverse.aux");
    //ReverseIndex reverseWayIndex(  storageDirectory + "wayReverse.idx",  storageDirectory +"wayReverse.aux");
    //ReverseIndex reverseRelationIndex(  storageDirectory +"relationReverse.idx",  storageDirectory +"relationReverse.aux");
    //mmap_t mapVertices = init_mmap("vertices.data", true, false);

    if (modeIsRandomAccess)
    {
        if (maxMmapSize != 0)
        {
            cerr << "error: Invalid parameter '--lock' for mode 'RANDOM'." << endl;
            cerr << "       Cannot lock memory in random access mode. Use 'BLOCK' mode instead." << endl;
            exit(EXIT_FAILURE);
        }
        /* big enough to always only require a single run (which is the point 
         * of 'random' mode), small enough to not cause an uint64_t overflow */
        maxMmapSize = 0xFFFFFFFFFFFFull; 
    } else {
        if (maxMmapSize == 0)
        {
            cerr << "error: missing parameter '--lock' for mode 'BLOCK'." << endl;
            exit(EXIT_FAILURE);
        }
    }

    FILE* fVertices = fopen( (storageDirectory + "vertices.data").c_str(), "rb");
    if (!fVertices)
    {
        std::cerr << "error: cannot open file '" + storageDirectory + "vertices.data'." << std::endl;
        exit(EXIT_FAILURE);
    }
    
    uint64_t pageSize = sysconf(_SC_PAGE_SIZE);
    //round *down* to nearest page size, as all memory limits are page-based
    maxMmapSize = (maxMmapSize / pageSize) * pageSize; 

    uint64_t vertexWindowSize = 0;
    uint64_t nVerticesPerRun  = 0;
    uint64_t nRuns =
        getRunConfiguration(fVertices, maxMmapSize, &vertexWindowSize, &nVerticesPerRun);

    
    if (modeIsRandomAccess)
        cout << "[INFO] Selected mode is 'RANDOM'" << endl;
    else
    {
        cout << "[INFO] Selected mode is 'BLOCK'." << endl; 
        cout << "       Will require " << nRuns << " iterations using " << (vertexWindowSize / 1000000) << "MB of RAM" << endl;
    }

    if (nRuns > 10 && !overrideRunLimit)
    {
        cerr << "error: current configuration would require more than 10 runs." << endl;
        cerr << "       This would take a long time and is probably not intended." << endl;
        cerr << "       Refusing to continue. Use parameter '--yes' to override." << endl;       
        exit( EXIT_FAILURE);
    }

    if (dryRun)
    {
        cout << "Tasked to only display stats, will exit now." << endl;
        exit( EXIT_SUCCESS);
    }
    

    if (! modeIsRandomAccess)
    {
    /* mode is 'BLOCK' where blocks of vertex data are kept in memory (i.e. are mlock()'ed).
       So we have to make sure that the current user is allow to mlock() as much memory
       as he requested using "--lock" */
       ensureMlockSize(maxMmapSize);
    }


    LightweightWayStore wayStore( (storageDirectory + "ways.idx").c_str(), 
                                  (storageDirectory + "ways.data").c_str(), true);

    for (uint64_t i = 0; i < nRuns; i++)
    {
        //uint64_t lastSynced = 0;
        if (nRuns > 1)
            cout << "[INFO] Starting run " << i << endl;
        
        if (!modeIsRandomAccess)
        {
            cout << "[INFO] Loading vertices for run " << i << " ...";
            cout.flush();
        }
        
        int mmapFlags = MAP_SHARED | (modeIsRandomAccess ? 0 : MAP_LOCKED);
        int32_t *vertexPos = (int32_t*) 
            mmap(NULL, vertexWindowSize, PROT_READ, mmapFlags,
                 fileno(fVertices), i * vertexWindowSize);

		if (vertexPos == MAP_FAILED)
			{perror("mmap error:"); exit(0);}

		int32_t *upper = vertexPos + (2 * nVerticesPerRun); //two int32 per vertex
		uint64_t firstVertexId = i*nVerticesPerRun;

        if (! modeIsRandomAccess)
            cout << "done." << endl;
        
        uint64_t pos = 0;
        for (OsmLightweightWay way : wayStore)
        {
	    //cout << way << endl;
            pos += 1;
            if (pos % 1000000 == 0)
            {
                cout << "[INFO] " << (pos / 1000000) << "M ways processed" << endl;
                /*cout << "syncing way range " << lastSynced << " -> " << (way.id-1);
                cout.flush();
                wayStore.syncRange(lastSynced, (way.id-1));
                cout << " done." << endl;
                lastSynced = way.id - 1;*/
            }
            for (OsmGeoPosition &node : way.getVertices() )
            //for (int j = 0; j < way.numVertices; j++)
            {
		        //not in the currently processed range
                if (node.id < (i*nVerticesPerRun) || node.id >= (i+1)*nVerticesPerRun)
                    continue;

                if ((node.lat == INVALID_LAT_LNG) != (node.lng == INVALID_LAT_LNG))
                {
                    cout << "invalid lat/lng pair " << (node.lat/10000000.0) << "/" << (node.lng/10000000.0) 
                         << " at node " << node.id << " in way " << way.id << endl;
                    exit(0); 
                }
                //MUST( (node.lat == INVALID_LAT_LNG) == (node.lng == INVALID_LAT_LNG), "invalid lat/lng pair" );
                if (node.lat != INVALID_LAT_LNG && node.lng != INVALID_LAT_LNG)
                    continue;   //already resolved to lat/lng pair
                
	            int32_t *base = (vertexPos + (node.id - firstVertexId)*2);
                /*if (base < vertexPos || base >= upper)
                {
                    cout << " invalid vertex pointer; should be " << vertexPos << "->" << upper << ", is " << base << endl;
                }*/
    	        MUST( base  >= vertexPos && base < upper, "index out of bounds");
                node.lat = base[0];
                node.lng = base[1];
                //    reverseNodeIndex.addReferenceFromWay( pos.id, way.id);
            }
            way.touch();

        }
        
        /*
        if ( 0 !=  madvise( vertexPos, vertexWindowSize, MADV_DONTNEED))
            perror("madvise error");*/
        munmap(vertexPos, vertexWindowSize);
    }

    /*
    cout << "[INFO] parsing relations" << endl;
    for (OsmRelation rel : relationStore)
    {
        for (OsmRelationMember mbr: rel.members)
        {
            switch (mbr.type)
            {
                case NODE: reverseNodeIndex.addReferenceFromRelation(mbr.ref, rel.id); break;
                case WAY: reverseWayIndex.addReferenceFromRelation(mbr.ref, rel.id);break;
                case RELATION: reverseRelationIndex.addReferenceFromRelation(mbr.ref, rel.id); break;
                default: assert(false && "invalid relation member"); break;
            }
        }
    }*/
    
    
    cout << "[INFO] Running validation pass." << endl;

    for (OsmLightweightWay way : wayStore)
    {
        for (int j = 0; j < way.numVertices; j++)
        {
            OsmGeoPosition pos = way.vertices[j]; 
            if (pos.lat == INVALID_LAT_LNG || pos.lng == INVALID_LAT_LNG)
            {
                cout << "ERROR: missed vertexId " << pos.id << " in way " << way.id << endl;
                MUST( false, "missed vertexId in way");
            }
            if (pos.lat > 900000000 || pos.lat < -900000000)
                cout << "invalid latitude " << pos.lat << " for node " << pos.id << " in way " << way.id << endl;
            if (pos.lng > 1800000000 || pos.lng < -1800000000)
                cout << "invalid longitude " << pos.lng << " for node " << pos.id << " in way " << way.id << endl;

        }
    }
    cout << "[INFO] Validation passed." << endl;
//    for (int i = 0; i < 30; i++)
//        cout << i << ": " << (reverseNodeIndex.isReferenced(i) ? "true" : "false") <<  endl;   
}

