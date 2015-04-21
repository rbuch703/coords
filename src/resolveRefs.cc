
#include <stdio.h>
#include <assert.h>

#include <iostream>
#include <vector>
#include <algorithm> //for sort()

#include "mem_map.h"
#include "osm/osmMappedTypes.h"
#include "containers/reverseIndex.h"
#include "containers/bucketFileSet.h"
#include "containers/osmWayStore.h"
#include "containers/osmRelationStore.h"

using namespace std;


template<typename T>
bool comparePairsByFirst(const T &a, const T &b) 
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

    BucketFileSet<OsmGeoPosition> resolvedNodeBuckets(storageDirectory +"nodeRefsResolved",
                                                      NODES_OF_WAYS_BUCKET_SIZE, false);
    BucketFileSet<uint64_t>       nodeBuckets(        storageDirectory +"nodeRefs", BUCKET_SIZE, true);

    for (uint64_t bucketId = 0; bucketId < nodeBuckets.getNumBuckets(); bucketId++)
    {
        cout << "resolving node locations for bucket " << (bucketId + 1) << "/" << nodeBuckets.getNumBuckets() << endl;
        
        vector<pair<uint64_t, uint64_t> > tuples = nodeBuckets.getContents(bucketId);
        sort( tuples.begin(), tuples.end(), comparePairsByFirst<pair<uint64_t, uint64_t> >);
        
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
            refs.insert( make_pair(wayId, map<uint64_t, pair<int32_t, int32_t> >()));
            
        if (! refs[wayId].count(nodeId))
            refs[wayId].insert( make_pair(nodeId, make_pair( tuple.second.lat, tuple.second.lng)));
        else 
            assert( refs[wayId][nodeId].first  == tuple.second.lat  && 
                    refs[wayId][nodeId].second == tuple.second.lng);
    }

    return refs;
}

void resolveNodeLocations(OsmLightweightWay &way, const map<uint64_t, pair<int32_t, int32_t> > &nodeRefs)
{
    assert(way.isDataMapped && "is noop for unmapped data");
    for (int i = 0; i < way.numVertices; i++)
    {
        uint64_t nodeId = way.vertices[i].id;
        if (! nodeRefs.count(nodeId))
        {
            cout << "[WARN] reference to node " << nodeId << " could not be resolved in way " << way.id << endl;
            continue;
        }
        
        const pair<int32_t, int32_t> &pos = nodeRefs.at(nodeId);
        way.vertices[i].lat = pos.first;
        way.vertices[i].lng = pos.second;
    }

}

void resolveWayNodeRefs(const string storageDirectory)
{
    ReverseIndex reverseWayIndex(storageDirectory +"wayReverse");
    LightweightWayStore wayStore( storageDirectory + "ways", true);
    BucketFileSet<OsmGeoPosition> resolvedNodeBuckets(storageDirectory +"nodeRefsResolved", NODES_OF_WAYS_BUCKET_SIZE, true);

    BucketFileSet<uint64_t> waysReferencedByRelationsBuckets(storageDirectory + "referencedWays", WAYS_OF_RELATIONS_BUCKET_SIZE, false);
    
    for (uint64_t i = 0; i < resolvedNodeBuckets.getNumBuckets(); i++)
    {
        cout << "resolving node references for bucket "<< (i+1) << "/" << resolvedNodeBuckets.getNumBuckets() << endl;
        /*TODO: the following call will take some seconds. The OS should be instructed to 
                write back all data from the previous iteration of the outer loop to disk
                during that time. */

        //for each wayId, mapping its nodeIds to the corresponding lat/lng pair
        map<uint64_t, map<uint64_t, pair<int32_t, int32_t> > > refs = buildReferencesMap( resolvedNodeBuckets.getContents(i) );
        
        for (uint64_t wayId =  i    * NODES_OF_WAYS_BUCKET_SIZE; 
                      wayId < (i+1) * NODES_OF_WAYS_BUCKET_SIZE; wayId++)
        {
            if (! wayStore.exists(wayId))
                continue;
                
            OsmLightweightWay way = wayStore[wayId];
            resolveNodeLocations(way, refs[wayId]);
            
            for (uint64_t relId : reverseWayIndex.getReferencingRelations(wayId))
                way.serialize( waysReferencedByRelationsBuckets.getFile(relId));
        }
        
        //clear bucket contents to save disk space
        resolvedNodeBuckets.clearBucket(i); 
    }
    
    resolvedNodeBuckets.clear(); // no longer needed, remove files;

}

void resolveRefsFromRelations(string storageDirectory) {

    {
        ReverseIndex reverseWayIndex(storageDirectory + "wayReverse", false);
        BucketFileSet<uint64_t> relationWayRefs(storageDirectory+"wayRefs", BUCKET_SIZE, true);
        
        for (uint64_t i = 0; i < relationWayRefs.getNumBuckets(); i++)
        {
            vector< pair<uint64_t, uint64_t> > refs = relationWayRefs.getContents(i);
            sort( refs.begin(), refs.end(), comparePairsByFirst< pair<uint64_t, uint64_t> >);
            
            for ( pair<uint64_t, uint64_t> kv : refs)
            {
                MUST(!( kv.second & IS_WAY_REFERENCE), "invalid way referencing another way");
                reverseWayIndex.addReferenceFromRelation(kv.first, kv.second);
            }
        }
        
        relationWayRefs.clear();
    }
   
    //register references from relations referencing other relations
    {
        ReverseIndex reverseRelationIndex(storageDirectory +"relationReverse", false);
        BucketFileSet<uint64_t> relationRelationRefs(storageDirectory+"relationRefs", BUCKET_SIZE, true);
        
        for (uint64_t i = 0; i < relationRelationRefs.getNumBuckets(); i++)
        {
            vector< pair<uint64_t, uint64_t> > refs = relationRelationRefs.getContents(i);
            sort( refs.begin(), refs.end(), comparePairsByFirst< pair<uint64_t, uint64_t> >);
            
            for ( pair<uint64_t, uint64_t> kv : refs)
            {
                MUST(!( kv.second & IS_WAY_REFERENCE), "invalid way referencing another way");
                reverseRelationIndex.addReferenceFromRelation(kv.first, kv.second);
            }
        }
        
        relationRelationRefs.clear();
    }
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

int main(int /*argc*/, char** /*argv*/)
{
    string storageDirectory = "intermediate/";
    
    if (storageDirectory.back() != '/' && storageDirectory.back() != '\\')
        storageDirectory += "/";

    cout << "Stage 0: adding dependencies from relations to bucket files" << endl;
    addRelationDependenciesToBucketFiles(storageDirectory);

    cout << "Stage 0a: Registering reverse dependencies for all relations" << endl;
    resolveRefsFromRelations(storageDirectory);
    
    cout << "Stage 1: Registering reverse dependencies" << endl;
    cout << "         and creating bucket files for resolved node locations." << endl;
    buildReverseIndexAndResolvedNodeBuckets(storageDirectory);
    
    cout << "Stage 2: Resolving node references for all ways" << endl;
    resolveWayNodeRefs(storageDirectory);

}

