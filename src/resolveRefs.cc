
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

struct RefTuple {
    uint64_t entityId;
    uint64_t refId;
};

int compareTuplesByRefId(const RefTuple* a, const RefTuple* b)
{
    return a->refId - b->refId;
}

struct WayResolvedNodeTuple {
    uint64_t wayId;
    uint64_t nodeId;
    int32_t  lat;
    int32_t  lng;
};

static const uint64_t RESOLVED_BUCKET_SIZE = 1000000;

/* reads all references from 'file', sorts them by refId, deletes the file, and returns the sorted array*/
vector<RefTuple> extractSortedReferences(const string fileName)
{
    FILE* fBucket = fopen(fileName.c_str(), "rb");
    if (!fBucket)
        return vector<RefTuple>();
    fseek(fBucket, 0, SEEK_END);
    uint64_t size = ftell(fBucket);
    fseek(fBucket, 0, SEEK_SET);
    MUST( size % (2*sizeof(uint64_t)) == 0, "bucket file corruption");
    uint64_t numTuples = size / (2*sizeof(uint64_t));
    RefTuple *tuples = new RefTuple[numTuples];

    cout << "creating reverse index for bucket "<< fileName << endl;
    MUST(1 == fread(tuples, size, 1, fBucket), "bucket read failed");
    fclose(fBucket);
    unlink( fileName.c_str());

    //cout << "sorting bucket " << (bucketId-1) << endl;
    qsort( tuples, numTuples, sizeof(RefTuple), (__compar_fn_t)compareTuplesByRefId);
    
    vector<RefTuple> res( tuples, tuples+numTuples);
    delete [] tuples;
    return res;


}

static const uint64_t IS_WAY_REFERENCE = 0x8000000000000000ull;

void buildReverseIndexAndResolvedNodeBuckets(const string storageDirectory)
{
    /* we don't actually need the reverse way and relation indices at this point. But since we
     * create and clear the reverse node index here, this is a good point to ensure that the
     * other indices are cleared as well */
    ReverseIndex reverseNodeIndex(storageDirectory + "nodeReverse", true);
    ReverseIndex reverseWayIndex(storageDirectory + "wayReverse", true);
    ReverseIndex reverseRelationIndex(storageDirectory +"relationReverse", true);
        
        
    std::vector<FILE*> resolvedNodeBuckets;
    mmap_t vertex_mmap = init_mmap( (storageDirectory + "vertices.data").c_str(), true, false);
    const int32_t *vertexData = (int32_t*)vertex_mmap.ptr;
    assert(vertex_mmap.size % (2*sizeof(int32_t)) == 0 && "vertex storage corruption");
    
    uint64_t numVertices = vertex_mmap.size / (2*sizeof(int32_t));

    uint64_t bucketId = 0;

    vector<RefTuple> tuples;
    while ( (tuples = extractSortedReferences(toBucketString(storageDirectory+"nodeRefs", bucketId++))).size() )
    {
        
        for ( RefTuple tuple: tuples)
        {
            if (! (tuple.entityId & IS_WAY_REFERENCE)) //is actually a reference from a relation
            {
                reverseNodeIndex.addReferenceFromRelation( tuple.refId, tuple.entityId);
                //nothing more to do; we do not actually resolve node references from relations at this point.
                continue;
            }
            
            tuple.entityId &= ~IS_WAY_REFERENCE; //unset way reference flag to obtain the actual way id
            reverseNodeIndex.addReferenceFromWay(
                tuple.refId, tuple.entityId);
            
            if (tuple.refId < numVertices)
            {
                WayResolvedNodeTuple tpl = {
                    .wayId  = tuple.entityId, 
                    .nodeId = tuple.refId, 
                    .lat = vertexData[ tuple.refId * 2],
                    .lng = vertexData[ tuple.refId * 2 + 1]};
                
                uint64_t bucketId = tpl.wayId / RESOLVED_BUCKET_SIZE;
                assert( bucketId < 800 && "getting too close to OS ulimit for open files");
                if (bucketId >= resolvedNodeBuckets.size())
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
        delete [] tuples;
        
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
        
        unlink(toBucketString(storageDirectory+"nodeRefsResolved", bucketId-1).c_str());
        
    }

}

void resolveRefsFromRelations(string storageDirectory) {
    ReverseIndex reverseWayIndex(storageDirectory + "wayReverse", false);
    
    uint64_t bucketId = 0;
    vector<RefTuple> tuples;
    while ( (tuples = extractSortedReferences(toBucketString(storageDirectory+"wayRefs", bucketId++))).size() )
    {
        for ( RefTuple tuple: tuples)
        {
            MUST(!(tuple.entityId & IS_WAY_REFERENCE), "invalid way referencing another way");
            reverseWayIndex.addReferenceFromRelation(tuple.refId, tuple.entityId);
        }
    }
    
    
    
     
    ReverseIndex reverseRelationIndex(storageDirectory +"relationReverse", false);
    bucketId = 0;
    while ( (tuples = extractSortedReferences(toBucketString(storageDirectory+"relationRefs", bucketId++))).size() )
    {
        for ( RefTuple tuple: tuples)
        {
            MUST(!(tuple.entityId & IS_WAY_REFERENCE), "invalid way referencing a relation");
            reverseRelationIndex.addReferenceFromRelation(tuple.refId, tuple.entityId);
        }
    }

        
}

void addReference(vector<FILE*> &buckets, uint64_t bucketSize, string storageDirectory, string fileBaseName, uint64_t relationId, uint64_t refId, bool appendToExistingBucketFile)
{
    uint64_t bucketId = refId / bucketSize;
    assert( bucketId < 800 && "getting too close to OS ulimit for open files");
    if (bucketId >= buckets.size() )
    {
        uint64_t oldNumBuckets = buckets.size();
        buckets.resize( bucketId + 1, nullptr);
        
        for (uint64_t i = oldNumBuckets; i < buckets.size(); i++)
            //*append* the relation->node ref to the way->node ref data already present
            buckets[i] = fopen( toBucketString(storageDirectory + fileBaseName, i).c_str(), appendToExistingBucketFile ? "ab" : "wb");
    }
    assert( buckets[bucketId]);
    uint64_t tpl[2] = {relationId, refId};
    MUST( fwrite( &tpl, sizeof(tpl), 1, buckets[bucketId]) == 1, "bucket write failed");
}

void addRelationDependenciesToBucketFiles(string storageDirectory)
{
    RelationStore relStore( storageDirectory + "relations");
    

    static const uint64_t BUCKET_SIZE = 10000000;
    vector<FILE*> nodeRefBuckets;
    vector<FILE*> wayRefBuckets;
    vector<FILE*> relationRefBuckets;
    
    /*  Note: This loop *adds* the references from relations to nodes to the *existing* nodeRef buckets, 
              which were created when 'coordsCreateStorage' registered the references from ways to nodes.
              But it *replaces* the wayRefXXXX and relationRefXXXX files, because none were created before,
              and any existing files with these names are leftovers from previous program executions (and
              their content would corrupt the data files if reused).
        
     */
    for (const OsmRelation & rel : relStore)
    {
        for (const OsmRelationMember &member : rel.members)
        {
            switch (member.type)
            {
                case NODE:         
                    addReference(nodeRefBuckets, BUCKET_SIZE, storageDirectory, "nodeRefs", rel.id, member.ref, true);
                    break;
                case WAY:
                    addReference(wayRefBuckets, BUCKET_SIZE, storageDirectory, "wayRefs", rel.id, member.ref, false);
                    break;
                case RELATION: 
                    addReference(relationRefBuckets, BUCKET_SIZE, storageDirectory, "relationRefs", rel.id, member.ref, false);
                    break;
                default: MUST(false, "invalid relation member type"); break;
            }
        }
    }

    /* try to delete the bucket files *after* the last one. 
     * Usually, these should not exist, and unlink() will just fail gracefully.
     * But in case there are leftover bucket files from an earlier run, this will
     * ensure that there is no unbroken sequence of files that contains both old 
     * and new bucket files. Thus, this deletion ensures that subsequent tools do not
     * use data from earlier runs.
     */
    unlink( toBucketString(storageDirectory+"nodeRefs", nodeRefBuckets.size()).c_str());
    unlink( toBucketString(storageDirectory+"wayRefs", nodeRefBuckets.size()).c_str());
    unlink( toBucketString(storageDirectory+"relationRefs", nodeRefBuckets.size()).c_str());
    
        
    
}

int main(int /*argc*/, char** /*argv*/)
{
    MUST(sizeof(WayResolvedNodeTuple) == 24, "data structure size mismatch");
    MUST(sizeof(RefTuple) == 2 * sizeof(uint64_t), "data structure size mismatch");

    string storageDirectory = "intermediate/";
    
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";

    cout << "Stage 0: adding dependencies from relations to bucket files" << endl;
    addRelationDependenciesToBucketFiles(storageDirectory);

    
    cout << "Stage 1: Registering reverse dependencies" << endl;
    cout << "         and creating bucket files for resolved node locations." << endl;
    buildReverseIndexAndResolvedNodeBuckets(storageDirectory);
    
    cout << "Stage 2: Resolving node references for all ways" << endl;
    resolveWayNodeRefs(storageDirectory);

    cout << "Stage 3: Registering reverse dependencies for all relations" << endl;
    resolveRefsFromRelations(storageDirectory);
}

