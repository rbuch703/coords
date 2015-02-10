
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <unistd.h> //for sysconf()
#include <sys/mman.h>
#include <sys/stat.h> //for stat()


#include "mem_map.h"
#include "osmMappedTypes.h"
#include "reverseIndex.h"

const uint64_t MAX_MMAP_SIZE = 8000ll * 1000 * 1000; //500 MB 

#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}


using namespace std;


int main()
{
    //ReverseIndex reverseNodeIndex("nodeReverse.idx", "nodeReverse.aux");
    //ReverseIndex reverseWayIndex(  "wayReverse.idx",  "wayReverse.aux");
    //ReverseIndex reverseRelationIndex(  "relationReverse.idx",  "relationReverse.aux");
    //mmap_t mapVertices = init_mmap("vertices.data", true, false);
    FILE* fVertices = fopen("vertices.data", "rb");
    fseek( fVertices, 0, SEEK_END);
    uint64_t numVertices = ftell(fVertices) / (2* sizeof(int32_t));
    //fclose(fVertices);
    
    uint64_t pageSize = sysconf(_SC_PAGE_SIZE);
    uint64_t maxNumConcurrentPages = MAX_MMAP_SIZE / pageSize;
    //the remaining code assumes that no vertex is spread across more than one page
    assert( pageSize % (2*sizeof(int32_t)) == 0); 
    uint64_t nVerticesPerPage = pageSize / (2*sizeof(int32_t));

    LightweightWayStore wayStore("intermediate/ways.idx", "intermediate/ways.data");
    //RelationStore relationStore("intermediate/relations.idx", "intermediate/relations.data");
    
    cout << "data set consists of " << (numVertices/1000000) << "M vertices" << endl;
    
    uint64_t numVertexBytes = numVertices * (2*sizeof(int32_t));
    uint64_t numVertexPages = (numVertexBytes+ (pageSize-1) ) / pageSize;  //rounded up
    cout << "data set size is " << (numVertexBytes/1000000) << "MB (" << numVertexPages << " pages)"<< endl;

    uint64_t nRuns = ( numVertexPages + (maxNumConcurrentPages - 1) ) / (maxNumConcurrentPages); //rounding up!
    assert( nRuns * MAX_MMAP_SIZE >= numVertexBytes);
    uint64_t nPagesPerRun = (numVertexPages + (nRuns -1)) / nRuns;  //rounded up to cover all vertices!
    uint64_t nVerticesPerRun =  nPagesPerRun * nVerticesPerPage;
    assert( nRuns * nVerticesPerRun >= numVertices);
    uint64_t vertexWindowSize = nVerticesPerRun * (2* sizeof(uint32_t));

    cout << "will process vertices in " << nRuns << " runs à " << nVerticesPerRun 
         << " vertices (" << (vertexWindowSize / 1000000) << "MB)" << endl;


    for (uint64_t i = 0; i < nRuns; i++)
    {
        //uint64_t lastSynced = 0;
        if (nRuns > 1)
            cout << "starting run " << i << endl;
        
        cout << "[DEBUG] paging in vertex range ... ";
        cout.flush();
        int32_t *vertexPos = (int32_t*) 
            mmap(NULL, vertexWindowSize, PROT_READ, MAP_SHARED | MAP_POPULATE,
                 fileno(fVertices), i * vertexWindowSize);
         cout << "done." << endl;
        
        uint64_t pos = 0;
        for (OsmLightweightWay way : wayStore)
        {
            pos += 1;
            if (pos % 1000000 == 0)
            {
                cout << (pos / 1000000) << "M ways processed" << endl;
                /*cout << "syncing way range " << lastSynced << " -> " << (way.id-1);
                cout.flush();
                wayStore.syncRange(lastSynced, (way.id-1));
                cout << " done." << endl;
                lastSynced = way.id - 1;*/
            }
            for (OsmGeoPosition &node : way.getVertices() )
            //for (int j = 0; j < way.numVertices; j++)
            {
                /* lat and lng are resolved at the same time, so either both values
                 * have to be valid, or none of them must be.*/ 
                MUST( (node.lat == INVALID_LAT_LNG) == (node.lng == INVALID_LAT_LNG), "invalid lat/lng pair" );
                if (node.lat != INVALID_LAT_LNG)
                    continue;   //already resolved to lat/lng pair
                
                if (node.id >= (i*nVerticesPerRun) && node.id < (i+1)*nVerticesPerRun)
                {
                    int32_t *base = (vertexPos - (i*nVerticesPerRun));
                    node.lat = base[2*node.id];
                    node.lng = base[2*node.id+1];
                //    reverseNodeIndex.addReferenceFromWay( pos.id, way.id);
                }
            }
            way.touch();

        }
        if ( 0 !=  madvise( vertexPos, vertexWindowSize, MADV_DONTNEED))
            perror("madvise");
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
    
    
    cout << "[INFO] running validation pass" << endl;

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
        }
    }
    cout << "passed." << endl;
//    for (int i = 0; i < 30; i++)
//        cout << i << ": " << (reverseNodeIndex.isReferenced(i) ? "true" : "false") <<  endl;   
}

