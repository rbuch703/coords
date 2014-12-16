
#include "osmConsumerIdRemapper.h"
#include <iostream>
#include <assert.h>

using namespace std;

OsmConsumerIdRemapper::OsmConsumerIdRemapper(OsmBaseConsumer *innerConsumer): 
    innerConsumer(innerConsumer)
{
    this->remap = new RemapEntry[64];
    this->remap[0] = RemapEntry(0, 0);
    this->numRemapEntries = 1;
    this->maxNumRemapEntries = 64;
    this->nextFreeId = 1;
    this->nextNodeId = 1;
    
}

OsmConsumerIdRemapper::~OsmConsumerIdRemapper()
{
    delete [] this->remap;
}


RemapEntry OsmConsumerIdRemapper::getRemapEntry(uint64_t nodeId)
{
    int maxPos = this->numRemapEntries-1;
    if (this->remap[maxPos].first < nodeId)
        return this->remap[maxPos];
    
    int minPos = 0;
    
    while (minPos + 1 < maxPos)
    {
        int midPos = (minPos + maxPos) / 2;
        if ( this->remap[midPos].first < nodeId)
            minPos = midPos;
        else maxPos = midPos;
    }
    
    if (minPos == maxPos)
    {
        assert (this->remap[maxPos].first < nodeId);
        return this->remap[maxPos];
    }

    assert (this->remap[minPos].first <= nodeId && this->remap[maxPos].first > nodeId);
    return this->remap[minPos];
}

void OsmConsumerIdRemapper::appendRemapEntry(uint64_t nodeId, int64_t offset)
{
    assert (nodeId > this->remap[this->numRemapEntries-1].first);
    
    if (this->numRemapEntries == this->maxNumRemapEntries)
    {
        RemapEntry* tmp = new RemapEntry[2* this->maxNumRemapEntries];
        for (int i = 0; i < this->maxNumRemapEntries; i++)
            tmp[i] == this->remap[i];
            
        delete [] this->remap;
        this->remap = tmp;
        this->maxNumRemapEntries *= 2;
    }
    
    this->remap[this->numRemapEntries++] = RemapEntry(nodeId, offset);
}

void OsmConsumerIdRemapper::consumeNode( OSMNode &node)
{
    //cout << node.id << endl;
    if (node.id != nextNodeId)
    {
        appendRemapEntry(node.id, nextFreeId);
    } //else

    nextNodeId = node.id + 1;
    node.id = nextFreeId;
    nextFreeId += 1;

    //cout << "found consecutive nodes" << endl;
    innerConsumer->consumeNode(node);
    
    //cout << "\t" << node.id << "->" << (nextFreeId)  << endl;
}

void OsmConsumerIdRemapper::consumeWay     ( OSMWay  &way) { innerConsumer->consumeWay(way);}
void OsmConsumerIdRemapper::consumeRelation( OSMRelation &relation) { innerConsumer->consumeRelation(relation);};

void OsmConsumerIdRemapper::onAllNodesConsumed () { innerConsumer->onAllNodesConsumed();}
void OsmConsumerIdRemapper::onAllWaysConsumed ()  { innerConsumer->onAllWaysConsumed(); }


void OsmConsumerIdRemapper::onAllRelationsConsumed ()
{
    innerConsumer->onAllRelationsConsumed();
    cout << "relocation table has " << this->numRemapEntries << " entries." << endl;
}

