
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <sys/mman.h>

#include "mem_map.h"
#include "osmTypes.h"
#include "reverseIndex.h"

const uint64_t MAX_MMAP_SIZE = 5000ll * 1000 * 1000; //500 MB 

#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}


using namespace std;

/** returns:
     whether 'pos' is resolved to a lat/lng pair. This is true, if either the 
     method has resolved the position itself, or if it was already resolved.
*
*/
bool tryResolveLatLng(OsmGeoPosition &pos, uint64_t minResolvableId, uint64_t beyondResolvableId, int32_t* vertexPos)
{
    /* lat and lng are resolved at the same time, so either both values
     * have to be valid, or none of them must be.*/ 
    MUST( (pos.lat == INVALID_LAT_LNG) == (pos.lng == INVALID_LAT_LNG), "invalid lat/lng pair" );
    if (pos.lat != INVALID_LAT_LNG)
        return true;   //already resolved to lat/lng pair
    
    uint64_t nodeId = pos.id;
    
    if (nodeId >= minResolvableId && nodeId < beyondResolvableId)
    {
        pos.lat = vertexPos[2*nodeId];
        pos.lng = vertexPos[2*nodeId+1];
        return true;
    }
    
    return false; 

}

int main()
{
    ReverseIndex reverseNodeIndex("nodeReverse.idx", "nodeReverse.aux");
    ReverseIndex reverseWayIndex(  "wayReverse.idx",  "wayReverse.aux");
    ReverseIndex reverseRelationIndex(  "relationReverse.idx",  "relationReverse.aux");
    mmap_t mapVertices = init_mmap("vertices.data", true, false);

    OsmLightweightWayStore wayStore("intermediate/ways.idx", "intermediate/ways.data");
    RelationStore relationStore("intermediate/relations.idx", "intermediate/relations.data");


    int32_t *vertexPos = (int32_t*)mapVertices.ptr;
         
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

    //cout << "vertex map address space: " << vertexPos << " (" << (mapVertices.size/1000000) << "MB)" << endl;
    for (uint64_t i = 0; i < nRuns; i++)
    {
        if (nRuns > 1)
        {
            cout << "starting run " << i << ", mlocking vertex range @" 
             << (vertexPos + i * vertexWindowSize) << " (" << (vertexWindowSize/1000000) << "MB) ...";
            cout.flush();
        }
/*        int res = mlock( vertexPos + i * vertexWindowSize, vertexWindowSize);

        if (res == 0)
            cout << " done." << endl;
        else
        {
            perror("mlock");
            cout << "mlock failed. You may not have the corresponding permissions or your ulimit may be set too low" << endl; 
        }*/
        
        for (OsmLightweightWay way : wayStore)
        {
            for (int j = 0; j < way.numVertices; j++)
            {
                tryResolveLatLng( way.vertices[j], i*nVerticesPerRun, (i+1)*nVerticesPerRun, vertexPos);
                reverseNodeIndex.addReferenceFromWay( way.vertices[j].id, way.id);

             }
        }
    }


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
    }
    
    
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

