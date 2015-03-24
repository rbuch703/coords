
#define __STDC_FORMAT_MACROS
#include <inttypes.h>   //for PRIu64
#include <stdio.h>
#include <assert.h>
#include <unistd.h> //for unlink()


//#include <getopt.h>
//#include <sys/mman.h>
#include <sys/stat.h> //for stat()
//#include <sys/resource.h>   //for getrlimit()

#include <iostream>
#include <vector>


#include "mem_map.h"
#include "osm/osmMappedTypes.h"
#include "containers/reverseIndex.h"

#ifndef MUST
    //a macro that is similar to assert(), but is not deactivated by NDEBUG
    #define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); abort();}}
#endif


using namespace std;

static string toBucketString(string baseName, uint64_t bucketId)
{
    char tmp [100];
    MUST( snprintf(tmp, 100, "%05" PRIu64, bucketId) < 100, "bucket id overflow");
    /* warning: do not attempt to integrate the following concatenation
     *          into the sprintf statement above. 'storageDirectory' has
     *          unbounded length, and thus may overflow any preallocated
     *          char array.*/
    return baseName + tmp + ".raw";
}

struct WayNodeTuple {
    uint64_t wayId;
    uint64_t nodeId;
};

int compareTupleByNodeId(const WayNodeTuple* a, const WayNodeTuple* b)
{
    return a->nodeId - b->nodeId;
}

struct WayResolvedNodeTuple {
    uint64_t wayId;
    uint64_t nodeId;
    int32_t  lat;
    int32_t  lng;
};

static const uint64_t RESOLVED_BUCKET_SIZE = 1000000;

void buildReverseIndexAndResolvedNodeBuckets(const string storageDirectory)
{
    ReverseIndex reverseNodeIndex(storageDirectory + "nodeReverse", true);
    //ReverseIndex reverseWayIndex(storageDirectory + "wayReverse", true);     
    //ReverseIndex reverseRelationIndex(storageDirectory +"relationReverse", true);
        
        
    std::vector<FILE*> resolvedNodeBuckets;
    mmap_t vertex_mmap = init_mmap( (storageDirectory + "vertices.data").c_str(), true, false);
    const int32_t *vertexData = (int32_t*)vertex_mmap.ptr;
    assert(vertex_mmap.size % (2*sizeof(int32_t)) == 0 && "vertex storage corruption");
    
    uint64_t numVertices = vertex_mmap.size / (2*sizeof(int32_t));

    uint64_t bucketId = 0;
    FILE* fBucket;
    while ( nullptr != (fBucket = fopen(
        toBucketString(storageDirectory+"nodeRefs", bucketId++).c_str(), "rb")) )
    {
        fseek(fBucket, 0, SEEK_END);
        uint64_t size = ftell(fBucket);
        fseek(fBucket, 0, SEEK_SET);
        MUST( size % (2*sizeof(uint64_t)) == 0, "bucket file corruption");
        uint64_t numTuples = size / (2*sizeof(uint64_t));
        WayNodeTuple *tuples = new WayNodeTuple[numTuples];
        
        cout << "creating reverse index for bucket "<< (bucketId-1) << endl;
        MUST(1 == fread(tuples, size, 1, fBucket), "bucket read failed");
        fclose(fBucket);
        
        //cout << "sorting bucket " << (bucketId-1) << endl;
        qsort( tuples, numTuples, sizeof(WayNodeTuple), (__compar_fn_t)compareTupleByNodeId);
        
        
        for (uint64_t i = 0; i < numTuples; i++)
        {
            reverseNodeIndex.addReferenceFromWay(
                tuples[i].nodeId, tuples[i].wayId);
            
            if (tuples[i].nodeId < numVertices)
            {
                WayResolvedNodeTuple tpl = {
                    .wayId  = tuples[i].wayId, 
                    .nodeId = tuples[i].nodeId, 
                    .lat = vertexData[ tuples[i].nodeId * 2],
                    .lng = vertexData[ tuples[i].nodeId * 2 + 1]};
                
                uint64_t bucketId = tpl.wayId / RESOLVED_BUCKET_SIZE;
                assert( bucketId < 800 && "getting too close to OS ulimit for open files");
                if (bucketId >= resolvedNodeBuckets.size() )
                {
                    uint64_t oldNumBuckets = resolvedNodeBuckets.size();
                    resolvedNodeBuckets.resize(bucketId+1, nullptr);
                    
                    for (uint64_t i = oldNumBuckets; i < resolvedNodeBuckets.size(); i++)
                        resolvedNodeBuckets[i] = fopen(
                        toBucketString(storageDirectory+"nodeRefsResolved", i).c_str(), "wb");
                }
                assert( resolvedNodeBuckets[bucketId]);
                MUST( fwrite( &tpl, sizeof(tpl), 1, resolvedNodeBuckets[bucketId]) == 1, "resolved bucket write failed");
            }
        }
        #warning necessary unlink() deactivated
        //unlink( toBucketString(storageDirectory+"nodeRefs", bucketId-1).c_str());
        
        delete [] tuples;
    }

    /* try to delete the bucket file *after* the last one. 
     * Usually, this one should not exist, and unlink() will just fail gracefully.
     * But in case there are leftover bucket files from an earlier run, this will
     * ensure that there is no unbroken sequence of files that contains both old 
     * and new bucket files. Thus, this deletion ensures that subsequent tools do not
     * use data from earlier runs.
     */
    unlink( toBucketString(storageDirectory+"nodeRefsResolved", resolvedNodeBuckets.size()).c_str());
    
    for (FILE* f : resolvedNodeBuckets)
        fclose(f);


}



void resolveWayNodeRefs(const string storageDirectory)
{
    LightweightWayStore wayStore( storageDirectory + "ways", true);

    uint64_t bucketId = 0;
    FILE* fBucket;
    while ( nullptr != (fBucket = fopen(
        toBucketString(storageDirectory+"nodeRefsResolved", bucketId++).c_str(), "rb")) )
    {
        //for each wayId, mapping its nodeIds to the corresponding lat/lng pair
        map<uint64_t, map<uint64_t, pair<int32_t, int32_t> > > refs; 

        cout << "resolving node references for bucket "<< (bucketId-1) << endl;

        fseek(fBucket, 0, SEEK_END);
        uint64_t size = ftell(fBucket);
        fseek(fBucket, 0, SEEK_SET);
        MUST( size % (sizeof( WayResolvedNodeTuple ) ) == 0, "bucket file corruption");
        uint64_t numTuples = size / (sizeof(WayResolvedNodeTuple));
        WayResolvedNodeTuple *tuples = new WayResolvedNodeTuple[numTuples];
        
        MUST(1 == fread(tuples, size, 1, fBucket), "bucket read failed");
        fclose(fBucket);
        //TODO: the following loop will take some seconds. The OS should be instructed to write back
        //      all data from the previous iteration of the outer loop to disk

        for (uint64_t i = 0; i < numTuples; i++)
        {
            const WayResolvedNodeTuple &tuple = tuples[i];
            if (! refs.count(tuple.wayId))
                refs.insert( make_pair(tuple.wayId, map<uint64_t, pair<int32_t, int32_t> >()));
                
            if (! refs[tuple.wayId].count(tuple.nodeId))
                refs[tuple.wayId].insert( make_pair(tuple.nodeId, make_pair( tuple.lat, tuple.lng)));
            else 
                assert( refs[tuple.wayId][tuple.nodeId].first  == tuple.lat  && 
                        refs[tuple.wayId][tuple.nodeId].second == tuple.lng);
        }
        
        for (uint64_t wayId = (bucketId-1) * RESOLVED_BUCKET_SIZE; wayId < bucketId * RESOLVED_BUCKET_SIZE; wayId++)
        {
            if (! wayStore.exists(wayId))
                continue;
                
            if (! refs.count(wayId))
            {
                cout << "[WARN] no node in way " << wayId << " could be resolved" << endl;
                continue;
            }
            
            OsmLightweightWay way = wayStore[wayId];
            assert(way.isDataMapped && "is noop for unmapped data");
            map<uint64_t, pair<int32_t, int32_t> > &nodeRefs = refs[wayId];
            for (int i = 0; i < way.numVertices; i++)
            {
                uint64_t nodeId = way.vertices[i].id;
                if (! nodeRefs.count(nodeId))
                {
                    cout << "[WARN] reference to node " << nodeId << " could not be resolved in way " << wayId << endl;
                    continue;
                }
                
                pair<int32_t, int32_t> &pos = nodeRefs[nodeId];
                way.vertices[i].lat = pos.first;
                way.vertices[i].lng = pos.second;
            }
        }
        
        #warning necessary unlink() deactivated
        //unlink(toBucketString(storageDirectory+"nodeRefsResolved", bucketId++).c_str())
        
    }

}


int main(int /*argc*/, char** /*argv*/)
{
    MUST(sizeof(WayResolvedNodeTuple) == 24, "data structure size mismatch");
    MUST(sizeof(WayNodeTuple) == 2 * sizeof(uint64_t), "data structure size mismatch");

    string storageDirectory = "intermediate/";
    
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";
    
    buildReverseIndexAndResolvedNodeBuckets(storageDirectory);
    resolveWayNodeRefs(storageDirectory);
    
}

