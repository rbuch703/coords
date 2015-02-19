
#include "osmConsumerIdRemapper.h"
#include <iostream>
#include <assert.h>

using namespace std;

typedef pair<uint64_t, int64_t> RemapEntry;

/*
class Remap {

public:
    Remap() {
        this->remap = new RemapEntry[64];
        this->remap[0] = RemapEntry(0, 0);
        this->numRemapEntries = 1;
        this->maxNumRemapEntries = 64;
        this->nextFreeId = 1;
        this->nextExpectedId = 1;
    }

    ~Remap() {
        delete [] this->remap;

    }
    
    uint64_t remapId(uint64_t id)
    {
        //cout << id << endl;
        if (id != nextExpectedId)
            this->appendRemapEntry(id);
        
        assert(getRemappedId(id) == nextFreeId);
        
        nextExpectedId = id + 1;
        return (nextFreeId++); //return the old value, increment afterwards 
    }
    
    uint64_t getNumEntries() const { return numRemapEntries;} 

    uint64_t getRemappedId(uint64_t id) const {
        RemapEntry re = getRemapEntry(id);
        int64_t offset = id - re.first;
        return re.second + offset;
    }

private:

    void appendRemapEntry(uint64_t id)
    {
        assert (id > this->remap[this->numRemapEntries-1].first);
        
        if (this->numRemapEntries == this->maxNumRemapEntries)
        {
            RemapEntry* tmp = new RemapEntry[2* this->maxNumRemapEntries];
            for (int i = 0; i < this->maxNumRemapEntries; i++)
                tmp[i] = this->remap[i];
                
            delete [] this->remap;
            this->remap = tmp;
            this->maxNumRemapEntries *= 2;
        }
        
        this->remap[this->numRemapEntries++] = RemapEntry(id, nextFreeId);
    }

    
    RemapEntry getRemapEntry(uint64_t id) const
    {
        int maxPos = this->numRemapEntries-1;
        if (this->remap[maxPos].first <= id)
            return this->remap[maxPos];
        
        int minPos = 0;
        
        while (minPos + 1 < maxPos)
        {
            int midPos = (minPos + maxPos) / 2;
            
            if (this->remap[midPos].first == id)
                return this->remap[midPos];
            
            if ( this->remap[midPos].first < id)
                minPos = midPos;
            else maxPos = midPos;
        }
        
        if (minPos == maxPos)
        {
            assert (this->remap[maxPos].first <= id);
            return this->remap[maxPos];
        }

        assert ( (this->remap[minPos].first <= id && this->remap[maxPos].first > id));
        return this->remap[minPos];
    }


private:
    RemapEntry *remap;
    int numRemapEntries;
    int maxNumRemapEntries;
    uint64_t nextFreeId;
    uint64_t nextExpectedId;

};*/

OsmConsumerIdRemapper::OsmConsumerIdRemapper(OsmBaseConsumer *innerConsumer): 
    innerConsumer(innerConsumer), nodeMap("intermediate/mapNodes.idx"),
    wayMap("intermediate/mapWays.idx"), relationMap("intermediate/mapRelations.idx")
    
{
    nodeIdsRemapped = nodeMap.getHighestValue();
    wayIdsRemapped = wayMap.getHighestValue();
    relationIdsRemapped = relationMap.getHighestValue();
           
}

OsmConsumerIdRemapper::~OsmConsumerIdRemapper ()
{
    cout << "relocation tables have " 
         << nodeIdsRemapped     << "/" 
         << wayIdsRemapped      << "/"
         << relationIdsRemapped << "/" << " entries." << endl;
}



void OsmConsumerIdRemapper::consumeNode( OSMNode &node)
{
    if (!nodeMap.count(node.id))
        nodeMap.insert( std::make_pair( node.id, ++nodeIdsRemapped));

    node.id = nodeMap[node.id];

    innerConsumer->consumeNode(node);
}

void OsmConsumerIdRemapper::consumeWay     ( OSMWay  &way) 
{ 
    if (!wayMap.count(way.id))
        wayMap.insert( std::make_pair( way.id, ++wayIdsRemapped));
        
    way.id = wayMap[way.id];
    
    for (OsmGeoPosition &pos : way.refs)
    {
        if (!nodeMap.count(pos.id))
            nodeMap.insert( std::make_pair(pos.id, ++nodeIdsRemapped));
        pos.id = nodeMap[pos.id];
    }

    //cout << way << endl;
    innerConsumer->consumeWay(way);
}

void OsmConsumerIdRemapper::consumeRelation( OsmRelation &relation) 
{ 

    if (!relationMap.count(relation.id))
        relationMap.insert( std::make_pair( relation.id, ++relationIdsRemapped));
    
    relation.id = relationMap[relation.id];


    for (OsmRelationMember &mbr : relation.members)
        switch (mbr.type)
        {
            case NODE: 
                if (! nodeMap.count(mbr.ref))
                    nodeMap.insert( std::make_pair(mbr.ref, ++nodeIdsRemapped));
                    
                mbr.ref = nodeMap[mbr.ref];
                break;
            case WAY: 
                if (!wayMap.count(mbr.ref))
                    wayMap.insert( std::make_pair( mbr.ref, ++wayIdsRemapped));
                
                mbr.ref = wayMap[mbr.ref];
                break;
            case RELATION: 
                if (!relationMap.count(mbr.ref))
                    relationMap.insert( std::make_pair( mbr.ref, ++relationIdsRemapped));
                
                mbr.ref = relationMap[mbr.ref];
                break;
            default: assert(false && "invalid member type"); break;
        }

    //consume only after remapping!
    innerConsumer->consumeRelation(relation);

};


