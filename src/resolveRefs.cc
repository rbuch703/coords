
#include <stdio.h>
#include <assert.h>

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <algorithm> //for sort()

#include "misc/mem_map.h"
#include "osm/osmMappedTypes.h"
#include "containers/chunkedFile.h"
#include "containers/reverseIndex.h"
#include "containers/bucketFileSet.h"
#include "containers/osmRelationStore.h"

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

void buildReverseIndexAndResolvedNodeBuckets(const string storageDirectory)
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
                reverseNodeIndex.addReferenceFromRelation( nodeId, wayId);
                //nothing more to do, as we do not actually resolve node references from relations at this point.
                continue;
            }
            
            wayId &= ~IS_WAY_REFERENCE; //unset way reference flag to obtain the actual way id
            reverseNodeIndex.addReferenceFromWay( nodeId, wayId);
            
            if (nodeId < numVertices)
            {
                OsmGeoPosition pos = {.id  = nodeId, .lat = vertexData[ nodeId * 2],.lng = vertexData[ nodeId * 2 + 1]};
                resolvedNodeBuckets.write( wayId, pos);
            }
        }
        nodeBuckets.clearBucket(bucketId);
    }
    nodeBuckets.clear();


}

//FIXME: this method uses ~60% of the whole CPU time of this program.
map<uint64_t, map<uint64_t, pair<int32_t, int32_t> > > buildReferencesMap(
    const vector< pair<uint64_t, OsmGeoPosition> > &resolvedReferences)
{
    //for each wayId, mapping its nodeIds to the corresponding lat/lng pair
    map<uint64_t, map<uint64_t, pair<int32_t, int32_t> > > refs; 

    for ( const pair<uint64_t, OsmGeoPosition> &tuple : resolvedReferences)
    {
        uint64_t wayId = tuple.first;
        uint64_t nodeId= tuple.second.id;

        if (! refs.count(wayId))
            refs.insert( std::make_pair(wayId, map<uint64_t, pair<int32_t, int32_t> >()));
            
        if (! refs[wayId].count(nodeId))
            refs[wayId].insert( std::make_pair(nodeId, std::make_pair( tuple.second.lat, tuple.second.lng)));
        else 
            assert( refs[wayId][nodeId].first  == tuple.second.lat  && 
                    refs[wayId][nodeId].second == tuple.second.lng);
    }

    return refs;
}

void resolveNodeLocations(OsmWay &way, const map<uint64_t, pair<int32_t, int32_t> > &nodeRefs)
{
    //assert(way.isDataMapped && "is noop for unmapped data");
    for (uint64_t i = 0; i < way.refs.size(); i++)
    {
        uint64_t nodeId = way.refs[i].id;
        if (! nodeRefs.count(nodeId))
        {
            cout << "[WARN] reference to node " << nodeId << " could not be resolved in way " << way.id << endl;
            continue;
        }
        
        const pair<int32_t, int32_t> &pos = nodeRefs.at(nodeId);
        way.refs[i].lat = pos.first;
        way.refs[i].lng = pos.second;
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
        map<uint64_t, map<uint64_t, pair<int32_t, int32_t> > > refs = buildReferencesMap( resolvedNodeBuckets.getContents(i) );
        
        
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

            resolveNodeLocations(way, refs[way.id]);

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
         * And boundary parsing is done later as part of the tiling process, so only multipolygons
		 * need to be considered here */
        if (rel.hasKey("type") && rel["type"] == "multipolygon")
            res.insert(rel.id);
    }
    return res; 
}

int main(int /*argc*/, char** /*argv*/)
{
    //622MB
    string storageDirectory = "intermediate/";
    
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";
    
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
    buildReverseIndexAndResolvedNodeBuckets(storageDirectory);
    
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

