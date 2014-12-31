
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>

#include "mem_map.h"
#include "osmTypes.h"

const uint64_t MAX_MMAP_SIZE = 5000ll * 1000 * 1000; //500 MB 

#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}


using namespace std;

int main()
{
    mmap_t mapVertices = init_mmap("vertices.data", true, false);
    mmap_t mapWayIndex = init_mmap("intermediate/ways.idx", true, false);
    mmap_t mapWayData  = init_mmap("intermediate/ways.data", true, true);

    int32_t *vertexPos = (int32_t*)mapVertices.ptr;
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    uint8_t  *wayData  = (uint8_t*)mapWayData.ptr;
    uint64_t numWays = mapWayIndex.size / sizeof(uint64_t);
         
    uint64_t numVertices = mapVertices.size / (2* sizeof(int32_t));
    
    cout << "data set consists of " << (numVertices/1000000) << "M vertices" << endl;
    
    uint64_t numVertexBytes = numVertices * (2*sizeof(int32_t));
    cout << "data set size is " << (numVertexBytes/1000000) << "MB" << endl;

    uint64_t nRuns = (numVertexBytes + MAX_MMAP_SIZE - 1) / (MAX_MMAP_SIZE); //rounding up!
    assert( nRuns * MAX_MMAP_SIZE >= numVertexBytes);
    uint64_t nVerticesPerRun = (numVertices + nRuns -1) / nRuns;
    assert( nRuns * nVerticesPerRun >= numVertices);
    cout << "will process vertices in " << nRuns << " runs à " << nVerticesPerRun << " vertices." << endl;

    uint64_t vertexWindowSize = nVerticesPerRun * (2* sizeof(uint32_t));

    cout << "vertex map address space: " << vertexPos << " (" << (mapVertices.size/1000000) << "MB)" << endl;
    for (uint64_t i = 0; i < nRuns; i++)
    {
        cout << "starting run " << i << ", mlocking vertex range @" 
             << (vertexPos + i * vertexWindowSize) << " (" << (vertexWindowSize/1000000) << "MB) ...";
        cout.flush();
        int res = mlock( vertexPos + i * vertexWindowSize, vertexWindowSize);

        if (res == 0)
            cout << " done." << endl;
        else
        {
            perror("mlock");
            cout << "mlock failed. You may not have the corresponding permissions or your ulimit may be set too low" << endl; 
        }
        
                
        for (uint64_t wayId = 0; wayId < numWays; wayId++)
        {
            uint64_t wayPos = wayIndex[wayId];
            if (wayPos == 0)
                continue;
                
            //fseek(fInput, wayPos, SEEK_SET);
            OsmLightweightWay way(wayData + wayPos, wayId);
            for (int j = 0; j < way.numVertices; j++)
            {
                OsmGeoPosition pos = way.vertices[j];
                /* lat and lng are resolved at the same time, so either both values
                 * have to be valid, or none of them must be.*/ 
                MUST( (pos.lat == INVALID_LAT_LNG) == (pos.lng == INVALID_LAT_LNG), "invalid lat/lng pair" );
                if (pos.lat != INVALID_LAT_LNG)
                    continue;   //already resolved to lat/lng pair
                
                uint64_t nodeId = pos.id;
                
                if (nodeId >= i*nVerticesPerRun && nodeId < (i+1)*nVerticesPerRun)
                {
                    way.vertices[j].lat = vertexPos[2*nodeId];
                    way.vertices[j].lng = vertexPos[2*nodeId+1];
                }
            }
        }
        
        munlockall();
    }
    
    cout << "running validation pass" << endl;

    for (uint64_t wayId = 0; wayId < numWays; wayId++)
    {
        uint64_t wayPos = wayIndex[wayId];
        if (wayPos == 0)
            continue;
            
        OsmLightweightWay way(wayData + wayPos, wayId);
        for (int j = 0; j < way.numVertices; j++)
        {
            OsmGeoPosition pos = way.vertices[j]; 
            if (pos.lat == INVALID_LAT_LNG || pos.lng == INVALID_LAT_LNG)
            {
                cout << "ERROR: missed vertexId " << pos.id << " in way " << wayId << endl;
                MUST( false, "missed vertexId in way");
            }
        }
    }
    cout << "passed." << endl;   
}

