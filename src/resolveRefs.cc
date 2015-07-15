
#include <stdio.h>
#include <getopt.h> //for getopt_long()
#include <assert.h>

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <algorithm> //for sort()

#include "misc/mem_map.h"
#include "misc/cleanup.h"
#include "containers/chunkedFile.h"
#include "containers/reverseIndex.h"
#include "containers/bucketFileSet.h"
#include "containers/osmRelationStore.h"
#include "geom/multipolygonReconstructor.h"

//using namespace std;
using std::string;
using std::cout;
using std::endl;
using std::vector;
using std::set;
using std::pair;
using std::map;

bool firstIsLess( const pair<uint64_t, uint64_t> &a, const pair<uint64_t, uint64_t> &b)
{
    return a.first < b.first;
}

/* TODO: 1. the stored wayIds in the nodeRefsResolved buckets are not used anymore, so do not
 *          write them to the file in the first place.
 *       2. the huge number of fwrite() calls to create the bucket files is rather inefficient.
 *          Create a buffered file class that collects some bucket contents (e.g. 16k) in memory
 *          before writing them out all at once.
 */
void buildReverseIndexAndResolvedNodeBuckets(const string storageDirectory, bool createReverseIndex)
{
    ReverseIndex reverseNodeIndex(storageDirectory + "nodeReverse", true);
        
    mmap_t vertex_mmap = init_mmap( (storageDirectory + "vertices.data").c_str(), true, false);
    const int32_t *vertexData = (int32_t*)vertex_mmap.ptr;
    MUST(vertex_mmap.size % (2*sizeof(int32_t)) == 0, "vertex storage corruption");
    uint64_t numVertices = vertex_mmap.size / (2*sizeof(int32_t));

    BucketFileSet<OsmGeoPosition> resolvedNodeBuckets(
        storageDirectory +"nodeRefsResolved", NODES_OF_WAYS_BUCKET_SIZE, false);
    BucketFileSet<uint64_t>       nodeBuckets(        
        storageDirectory +"nodeRefs", BUCKET_SIZE, true);

    for (uint64_t bucketId = 0; bucketId < nodeBuckets.getNumBuckets(); bucketId++)
    {
        cout << "resolving node locations for bucket " << (bucketId + 1) << "/" << nodeBuckets.getNumBuckets() << endl;
        
        vector<pair<uint64_t, uint64_t> > tuples = nodeBuckets.getContents(bucketId);
        sort( tuples.begin(), tuples.end(), firstIsLess);
        
        for ( pair<uint64_t, uint64_t> &tuple: tuples)
        {
            uint64_t nodeId = tuple.first;
            uint64_t wayId  = tuple.second;
            if (! (wayId & IS_WAY_REFERENCE)) //is actually a reference from a relation
            {
                if (createReverseIndex)
                    reverseNodeIndex.addReferenceFromRelation( nodeId, wayId);
                //nothing more to do, as we do not actually resolve node references from relations at this point.
                continue;
            }
            
            wayId &= ~IS_WAY_REFERENCE; //unset way reference flag to obtain the actual way id
            if (createReverseIndex)
                reverseNodeIndex.addReferenceFromWay( nodeId, wayId);
            
            if (nodeId < numVertices)
            {
                OsmGeoPosition pos = {.id  = nodeId, .lat = vertexData[ nodeId * 2],.lng = vertexData[ nodeId * 2 + 1]};
                FILE* f = resolvedNodeBuckets.getFile(wayId);
                MUST( fwrite( &pos, sizeof(pos), 1, f) == 1, "write error");
                //resolvedNodeBuckets.write( wayId, pos);
            }
        }
        nodeBuckets.clearBucket(bucketId);
    }
    nodeBuckets.clear();


}

inline bool hasSmallerNodeId( const OsmGeoPosition &a, const OsmGeoPosition &b)
{
    return a.id < b.id;
}

inline bool isSmallerNodeId( const OsmGeoPosition &a, const uint64_t &nodeId)
{
    return a.id < nodeId;
}

vector<OsmGeoPosition> buildReferencesMap(FILE* bucketFile)
{
    fseek (bucketFile, 0, SEEK_END);
    uint64_t size = ftell(bucketFile);
    rewind(bucketFile);
    
    MUST( size % sizeof(OsmGeoPosition) == 0, "resolved node ref bucket file corruption");
    uint64_t numItems = size / sizeof(OsmGeoPosition);
    
    vector<OsmGeoPosition> refs;
    refs.resize( numItems);
    
    MUST( fread( refs.data(), size, 1, bucketFile) == 1, "read error");
    
    sort( refs.begin(), refs.end(), hasSmallerNodeId);
    
    return refs;
}

void resolveNodeLocations(OsmWay &way, const vector<OsmGeoPosition> &nodeRefs)
{
    //assert(way.isDataMapped && "is noop for unmapped data");
    for (uint64_t i = 0; i < way.refs.size(); i++)
    {
        uint64_t nodeId = way.refs[i].id;
        
        /* returns the first element in the sorted 'nodeRefs' vector that is not
         * smaller than 'nodeId'. This will be an entry containing 'nodeId', if such
         * an entry exists, and one with a higher id than 'nodeId' otherwise */
        auto notSmaller = lower_bound( nodeRefs.begin(), nodeRefs.end(), nodeId, isSmallerNodeId);
        
        if ( notSmaller == nodeRefs.end() || notSmaller->id != nodeId)
        {
            cout << "[WARN] reference to node " << nodeId << " could not be resolved in way " << way.id << endl;
            continue;
        }
        
        way.refs[i].lat = notSmaller->lat;
        way.refs[i].lng = notSmaller->lng;
    }

}

void resolveWayNodeRefsAndCreateRelationBuckets(const string storageDirectory, 
                        const set<uint64_t> &rendereableRelationIds)
{

    ReverseIndex reverseWayIndex(storageDirectory +"wayReverse");
    
    BucketFileSet<OsmGeoPosition> resolvedNodeBuckets(
                storageDirectory +"nodeRefsResolved",
                NODES_OF_WAYS_BUCKET_SIZE, true);
    
    BucketFileSet<void*> wayBuckets( 
                storageDirectory + "ways", 
                NODES_OF_WAYS_BUCKET_SIZE, true);

    BucketFileSet<uint64_t> waysReferencedByRelationsBuckets(
                storageDirectory + "referencedWays", 
                WAYS_OF_RELATIONS_BUCKET_SIZE, false);
    
    ChunkedFile waysStorage(storageDirectory + "ways.data");
    mmap_t waysIndex = init_mmap( (storageDirectory + "ways.idx").c_str(), true, true, true);
    MUST( resolvedNodeBuckets.getNumBuckets() == wayBuckets.getNumBuckets(), "bucket count mismatch")
    for (uint64_t i = 0; i < resolvedNodeBuckets.getNumBuckets(); i++)
    {
        cout << "resolving node references for bucket "<< (i+1) << "/" << resolvedNodeBuckets.getNumBuckets() << endl;
        /*TODO: the following call will take some seconds. The OS should be instructed to 
                write back all data from the previous iteration of the outer loop to disk
                during that time. */

        //for each wayId, mapping its nodeIds to the corresponding lat/lng pair
        vector<OsmGeoPosition> refs = buildReferencesMap(resolvedNodeBuckets.getFile( 
                                                            i*NODES_OF_WAYS_BUCKET_SIZE));
        
        FILE* f = wayBuckets.getFile( i * NODES_OF_WAYS_BUCKET_SIZE);
        fseek (f, 0, SEEK_END);
        uint64_t numWayBytes = ftell(f);
        fseek(f, 0, SEEK_SET);
        uint8_t *waysRaw = new uint8_t[numWayBytes];
        MUST( fread( waysRaw, numWayBytes, 1, f) == 1, "way bucket read error");

        const uint8_t* waysPos = waysRaw;
        const uint8_t* waysBeyond = waysRaw + numWayBytes;
        
        while (waysPos < waysBeyond)
        {
            OsmWay way(waysPos);
            MUST( way.id >=  i   * NODES_OF_WAYS_BUCKET_SIZE &&
                  way.id <  (i+1)* NODES_OF_WAYS_BUCKET_SIZE, "Way in wrong bucket file");

            resolveNodeLocations(way, refs);

            for (uint64_t relId : reverseWayIndex.getReferencingRelations(way.id))
                if (rendereableRelationIds.count(relId))
                    way.serialize( waysReferencedByRelationsBuckets.getFile(relId));

            ensure_mmap_size( &waysIndex, sizeof(uint64_t) * (way.id+1));
            uint64_t *index = (uint64_t*)waysIndex.ptr;
                    
            uint64_t numBytes = 0;
            uint8_t* wayBytes = way.serialize( &numBytes);
            Chunk chunk = waysStorage.createChunk( numBytes);
            index[way.id] = chunk.getPositionInFile();
            chunk.put( wayBytes, numBytes);

            delete [] wayBytes;
        }
        MUST( waysPos == waysBeyond, "overflow");
        delete [] waysRaw;
        resolvedNodeBuckets.clearBucket(i); 
        wayBuckets.clearBucket(i);
    }
    
    resolvedNodeBuckets.clear(); // no longer needed, remove files;
    wayBuckets.clear();

}

void registerWayRefsFromRelations(string storageDirectory) 
{
    ReverseIndex reverseWayIndex(storageDirectory + "wayReverse", false);
    BucketFileSet<uint64_t> relationWayRefs(storageDirectory+"wayRefs", BUCKET_SIZE, true);
    
    for (uint64_t i = 0; i < relationWayRefs.getNumBuckets(); i++)
    {
        vector< pair<uint64_t, uint64_t> > refs = relationWayRefs.getContents(i);
        sort( refs.begin(), refs.end(), firstIsLess);
        
        for ( pair<uint64_t, uint64_t> kv : refs)
        {
            MUST(!( kv.second & IS_WAY_REFERENCE), "invalid way referencing another way");
            reverseWayIndex.addReferenceFromRelation(kv.first, kv.second);
        }
    }
    
    relationWayRefs.clear();
}

void registerRelationRefsFromRelations(string storageDirectory) 
{
    //register references from relations referencing other relations
    ReverseIndex reverseRelationIndex(storageDirectory +"relationReverse", false);
    BucketFileSet<uint64_t> relationRelationRefs(storageDirectory+"relationRefs", BUCKET_SIZE, true);
    
    for (uint64_t i = 0; i < relationRelationRefs.getNumBuckets(); i++)
    {
        vector< pair<uint64_t, uint64_t> > refs = relationRelationRefs.getContents(i);
        sort( refs.begin(), refs.end(), firstIsLess);
        
        for ( pair<uint64_t, uint64_t> kv : refs)
        {
            MUST(!( kv.second & IS_WAY_REFERENCE), "invalid way referencing another way");
            reverseRelationIndex.addReferenceFromRelation(kv.first, kv.second);
        }
    }
    
    relationRelationRefs.clear();
}


void addRelationDependenciesToBucketFiles(string storageDirectory)
{
    RelationStore relStore( storageDirectory + "relations");

    /*  Note: We *append* references from relations to nodes to the *existing* nodeRef buckets, 
              which were created when 'coordsCreateStorage' registered the references from ways
              to nodes.
              But we *replace* the wayRefXXXX and relationRefXXXX files, because none were
              created before, and thus any existing files with these names are leftovers from
              previous program executions (and their content would corrupt the data files if
              reused). */
    BucketFileSet<uint64_t> nodeRefBuckets(storageDirectory+"nodeRefs", BUCKET_SIZE, true);
    BucketFileSet<uint64_t> wayRefBuckets (storageDirectory+"wayRefs",  BUCKET_SIZE, false);
    BucketFileSet<uint64_t> relationRefBuckets(storageDirectory+"relationRefs", BUCKET_SIZE, false);
    
    for (const OsmRelation & rel : relStore)
    {
           
        for (const OsmRelationMember &member : rel.members)
        {
            switch (member.type)
            {
                case OSM_ENTITY_TYPE::NODE:     
                    nodeRefBuckets.write(    member.ref, rel.id);
                    break;
                case OSM_ENTITY_TYPE::WAY:      
                    wayRefBuckets.write(     member.ref, rel.id);   
                    break;
                case OSM_ENTITY_TYPE::RELATION: 
                    relationRefBuckets.write(member.ref, rel.id);   
                    break;
                default: MUST(false, "invalid relation member type"); break;
            }
        }
    }    
}

std::set<uint64_t> getRenderableRelationIds(const string &storageDirectory)
{
    std::set<uint64_t> res;
    
    for (const OsmRelation & rel : RelationStore( storageDirectory + "relations"))
    {
        /* The only relation types that affect rendering are multipolygons and boundaries.
         * And boundary parsing is done later as part of the tiling process, so only
         * multipolygons need to be considered here */
        if (rel.hasKey("type") && rel["type"] == "multipolygon")
            res.insert(rel.id);
    }
    return res; 
}

std::string storageDirectory;
bool keepReverseIndexFiles = true;
bool assembleMultipolygons = true;
bool resolveReferences = true;
bool keepWayBuckets = false;

void parseArguments(int argc, char** argv, const std::string &usageLine)
{
    static const struct option long_options[] =
    {
//        {"keep-way-buckets", no_argument, NULL, 'k'},
        {"no-multipolygons", no_argument, NULL, 'm'},
        {"no-resolve",       no_argument, NULL, 'r'},
        {"no-updates",       no_argument, NULL, 'u'},
        {0,0,0,0}
    };

    int opt_idx = 0;
    int opt;
    while (-1 != (opt = getopt_long(argc, argv, "mru", long_options, &opt_idx)))
    {
        switch(opt) {
            case '?': exit(EXIT_FAILURE); break; //unknown option; getopt_long() already printed an error message
//            case 'k': keepWayBuckets = true; break;
            case 'm': assembleMultipolygons = false; break;
            case 'r': resolveReferences = false; break;
            case 'u': keepReverseIndexFiles = false; break;
            default: abort(); break;
        }
    }
    
    if (optind < argc-1)
    {
        cout << "too many arguments." << endl;
        cout << usageLine << endl;
        exit(EXIT_FAILURE);
    }
    
    if (optind > argc-1)
    {
        cout << optind << ", " << argc << endl;
        cout << "missing argument <storage directory>." << endl;
        cout << usageLine << endl;
        exit(EXIT_FAILURE);
    }
    
    storageDirectory = argv[optind];

    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";

}

int main(int argc, char** argv)
{
    std::string usageLine = std::string("usage: ") + argv[0] + /*" [-k|--keep-way-buckets]*/" [-m|--no-multipolygons] [-r|--no-resolve] [-u|--no-updates] <storage directory>";
    parseArguments(argc, argv, usageLine);

    if (resolveReferences)
    {
        cout << "Stage 1: determining set of multipolygon relations" << endl;
        /* Input: all relations
         * Output: set of relation IDs for relations that are either multipolygons or boundaries
         *         (and thus which may affect rendering)
         */
        std::set<uint64_t> renderableRelations = getRenderableRelationIds(storageDirectory);

        cout << "Stage 2: adding dependencies from relations to bucket files" << endl;
        /* Input:  all relations
         * Output: updated dependency bucket files, now containing also an entry for each 
         *         node, way and relation that each relation refers to. (coordsCreateStorage
         *         only added entries for each node referred to by each way.
         *
        */
        addRelationDependenciesToBucketFiles(storageDirectory);

        cout << "Stage 3: Registering reverse dependencies for all relations" << endl;
        /* Input:  - "relation->way" and "relation->relation" bucket files.
         * Output: - reverse dependency files for ways and relations
         *         - the input files are destroyed */
        registerWayRefsFromRelations(storageDirectory);
        registerRelationRefsFromRelations(storageDirectory);
        
        cout << "Stage 4: Registering reverse dependencies" << endl;
        cout << "         and creating bucket files for resolved node locations." << endl;
        /* Input: - vertex data file 
         *        - node buckets ( (nodeId, wayId) tuples, bucketed by *nodeId*)
         * Output: - reverse dependency file for nodes (which ways and relations refer to each node)
         *         - resolved node buckets (nodeId, nodePos, wayId) tuples, bucketed by *wayId*)
         *         - the input node buckets are destroyed */
        buildReverseIndexAndResolvedNodeBuckets(storageDirectory, keepReverseIndexFiles);
        
        cout << "Stage 5: Resolving node references for all ways" << endl;
        /* Input: - resolved node buckets
         *        - all ways
         * Output: - updated ways files where the each node ref in each way has been
         *           augmented by the actual node lat/lng position
         *         - "referencedWays" bucket files: for each way referenced by a multipolygon or 
         *           boundary relation an entry containing the full serialized way, bucketed by
         *           relation id (used later for multipolygon reconstruction)
         *         - the input resolved node buckets are destroyed
         */
        resolveWayNodeRefsAndCreateRelationBuckets(storageDirectory, renderableRelations);
    }
    
    if (assembleMultipolygons)
    {
        cout << "Stage 6: assembling multipolygons" << endl;
        FILE* fOut = fopen( (storageDirectory + "multipolygons.bin").c_str(), "wb");
        MUST( fOut, "cannot open output file");
        
        std::vector<uint64_t> vOuterWayIds = buildMultipolygonGeometry(storageDirectory, fOut, true);
        fclose(fOut);
        
        fOut = fopen((storageDirectory + "outerWayIds.bin").c_str(), "wb");
        MUST( fwrite( vOuterWayIds.data(), sizeof(uint64_t) * vOuterWayIds.size(), 1, fOut) == 1,
              "write error");
        fclose(fOut);
    }

    if (!keepReverseIndexFiles)
    {
        deleteIfExists(storageDirectory, "nodeReverse.aux");
        deleteIfExists(storageDirectory, "nodeReverse.idx");
        deleteIfExists(storageDirectory, "wayReverse.aux");
        deleteIfExists(storageDirectory, "wayReverse.idx");
        deleteIfExists(storageDirectory, "relationReverse.aux");
        deleteIfExists(storageDirectory, "relationReverse.idx");
    }    
    
}

