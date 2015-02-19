
#include "osmConsumerIdRemapper.h"
#include <iostream>
#include <assert.h>

using namespace std;

typedef pair<uint64_t, int64_t> RemapEntry;

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

};

OsmConsumerIdRemapper::OsmConsumerIdRemapper(OsmBaseConsumer *innerConsumer): 
    innerConsumer(innerConsumer)
{
    nodeRemap = new Remap();
    wayRemap = new Remap();
    relationRemap = new Remap();
    
}

OsmConsumerIdRemapper::~OsmConsumerIdRemapper()
{
    delete nodeRemap;
    delete wayRemap;
    delete relationRemap;
}



void OsmConsumerIdRemapper::consumeNode( OSMNode &node)
{
    node.id = nodeRemap->remapId(node.id);
    innerConsumer->consumeNode(node);
}

void OsmConsumerIdRemapper::consumeWay     ( OSMWay  &way) 
{ 
    way.id = wayRemap->remapId(way.id);
    
    for (OsmGeoPosition &pos : way.refs)
        pos.id = this->nodeRemap->getRemappedId(pos.id);

    //cout << way << endl;
    innerConsumer->consumeWay(way);

}

void OsmConsumerIdRemapper::consumeRelation( OsmRelation &relation) 
{ 
    /* WARNING: we cannot reliably remap relation IDs in a single pass:
     *          The current remapping process requires all relation IDs to be remapped
     *          to be presented in increasing order. This is true for the IDs
     *          of the relations to be consumed themselves; but it is not necessarily
     *          true for the IDs of their members of type 'relation'.
     *          So we do not remap relation IDs for now. Fortunately, even the full set
     *          of relation IDs is so small (usually ~1/1000th) compared to the set of nodes or ways that
     *          even the index for all non-remapped relation IDs usually does not dominate
     *          the storage requirements.
     */

    for (OsmRelationMember &mbr : relation.members)
        switch (mbr.type)
        {
            case NODE: mbr.ref = this->nodeRemap->getRemappedId(mbr.ref); break;
            case WAY: mbr.ref = this->wayRemap->getRemappedId(mbr.ref); break;
            case RELATION: break; //NOOP, since we donot remap relation IDs
            default: assert(false && "invalid member type"); break;
        }

    //consume only after remapping!
    innerConsumer->consumeRelation(relation);

};

void OsmConsumerIdRemapper::onAllNodesConsumed () { innerConsumer->onAllNodesConsumed();}
void OsmConsumerIdRemapper::onAllWaysConsumed ()  { innerConsumer->onAllWaysConsumed(); }


void OsmConsumerIdRemapper::onAllRelationsConsumed ()
{
    innerConsumer->onAllRelationsConsumed();
    cout << "relocation tables have " << this->nodeRemap->getNumEntries() 
         << "/" << this->wayRemap->getNumEntries() << " entries." << endl;
}

