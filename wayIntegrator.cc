
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>

#include "mem_map.h"
#include "osmTypes.h"

const uint64_t MAX_MMAP_SIZE = 500 * 1000 * 1000; //500 MB 

using namespace std;

int main()
{
    mmap_t mapVertices = init_mmap("vertices.data", true, false);
    mmap_t mapWayIndex = init_mmap("intermediate/ways.idx", true, false);
    mmap_t mapWayData  = init_mmap("intermediate/ways.data", true, true);

    OsmGeoPosition *vertexPos = (OsmGeoPosition*)mapVertices.ptr;
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    uint8_t  *wayData  = (uint8_t*)mapWayData.ptr;
    uint64_t numWays = mapWayIndex.size / sizeof(uint64_t);
         
    uint64_t numVertices = mapVertices.size / sizeof (OsmGeoPosition);
    
    cout << "data set consists of " << (numVertices/1000000) << "M vertices" << endl;
    
    uint64_t numVertexBytes = numVertices * sizeof(OsmGeoPosition);
    cout << "data set size is " << (numVertexBytes/1000000) << "MB" << endl;

    uint64_t nRuns = (numVertexBytes + MAX_MMAP_SIZE - 1) / (MAX_MMAP_SIZE); //rounding up!
    assert( nRuns * MAX_MMAP_SIZE >= numVertexBytes);
    uint64_t nVerticesPerRun = (numVertices + nRuns -1) / nRuns;
    assert( nRuns * nVerticesPerRun >= numVertices);
    cout << "will process vertices in " << nRuns << " runs à " << nVerticesPerRun << " vertices." << endl;

    //FILE *fWaysIn  = fopen("intermediate/ways.data", "rb");
    //FILE *fOutput = fopen("intermediate/ways_int.data", "wb+");

    for (uint64_t i = 0; i < nRuns; i++)
    {
        cout << "starting run " << i << ", mlocking vertex range ...";
        cout.flush();
        uint64_t vertexWindowSize = nVerticesPerRun * sizeof(OsmGeoPosition);
        mlock( vertexPos + i * vertexWindowSize, vertexWindowSize);
        cout << " done." << endl;
        
        //first run has to stream from input to output; all other runs just modify the existing output
        //FILE* fInput = (i == 0) ? fWaysIn : fOutput;
        
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
                bool msb = (pos.geo.lat & (1 << 31));
                bool smsb= (pos.geo.lat & (1 << 30));
                bool containsId = msb != smsb;
//                cout << way << endl;
//                assert(containsId);
                if (!containsId)
//                {
//                    cout << (way.vertices[j].geo.lat / 10000000.0) << "° / "
//                         << (way.vertices[j].geo.lng / 10000000.0) << "°" << endl;

                    continue;   //already resolved to lat/lng pair
//                }
                
                pos.geo.lat ^= 0x80000000;  //unset marker bit to retrieve actual id
                uint64_t nodeId = pos.id;
                
                if (nodeId >= i*nVerticesPerRun && nodeId < (i+1)*nVerticesPerRun)
                {
                    way.vertices[j] = vertexPos[nodeId];
//                    cout << (way.vertices[i].geo.lat / 10000000.0) << "° / "
//                         << (way.vertices[i].geo.lng / 10000000.0) << "°" << endl;
                }
//                cout << "# " << wayId << endl;;
            }
            //cout << "@" << wayPos << ": " << way << endl;
            
            //fseek(fOutput, wayPos, SEEK_SET);
            //way.serialize(fOutput);
                
//            cout << "way " << wayId << " is stored at index " << wayPos << endl;
        }
        
        munlockall();
//        exit(0);
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
            bool msb = (pos.geo.lat & (1 << 31));
            bool smsb= (pos.geo.lat & (1 << 30));
            if (msb != smsb)
                cout << "ERROR: missed vertexId in way" << wayId << endl;
        }
    }
    cout << "passed." << endl;
    
}
