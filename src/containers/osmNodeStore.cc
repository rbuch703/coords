
#include "osmNodeStore.h"
#include <sys/mman.h>

OsmNodeStore::OsmNodeStore(const char* indexFileName, const char* dataFileName, bool optimizeForStreaming) {
    mapNodeIndex = init_mmap(indexFileName, true, false);
    mapNodeData  = init_mmap(dataFileName, true, false);

    if (optimizeForStreaming)
    {
        madvise( mapNodeData.ptr,  mapNodeData.size,  MADV_SEQUENTIAL);
        madvise( mapNodeIndex.ptr, mapNodeIndex.size, MADV_SEQUENTIAL);
    }

}

OsmNodeStore::OsmNodeStore(const std::string &baseName, bool optimizeForStreaming):
    OsmNodeStore((baseName + ".idx").c_str(), (baseName +".data").c_str(), optimizeForStreaming)
{}


OsmNode OsmNodeStore::operator[](uint64_t nodeId)
{
    uint64_t *nodeIndex = (uint64_t*)mapNodeIndex.ptr;
    assert(nodeIndex[nodeId] != 0 && "trying to access non-existent node");
    uint64_t nodePos = nodeIndex[nodeId];
    return OsmNode((uint8_t*)mapNodeData.ptr + nodePos);
}

bool OsmNodeStore::exists(uint64_t nodeId) const
{
    uint64_t *nodeIndex = (uint64_t*)mapNodeIndex.ptr;
    if (nodeId >= getMaxNumNodes())
        return false;
        
    return nodeIndex[nodeId] != 0;
}




OsmNodeStore::NodeIterator OsmNodeStore::begin() 
{ 
    return NodeIterator(*this, 0);
}

OsmNodeStore::NodeIterator OsmNodeStore::end()   
{ 
    return NodeIterator(*this, getMaxNumNodes());
}

uint64_t OsmNodeStore::getMaxNumNodes() const 
{ 
    return mapNodeIndex.size / sizeof(uint64_t); 
}


OsmNodeStore::NodeIterator::NodeIterator( OsmNodeStore &host, uint64_t pos):
    host(host), pos(pos) 
{
    advanceToNextNode();
}

OsmNodeStore::NodeIterator& OsmNodeStore::NodeIterator::operator++() {
    pos++;
    advanceToNextNode();
    return *this;
}

OsmNode OsmNodeStore::NodeIterator::operator *() {
    return host[pos];
}

bool OsmNodeStore::NodeIterator::operator !=( NodeIterator &other) const
{ 
    return pos != other.pos;
} 

void OsmNodeStore::NodeIterator::advanceToNextNode() {
    uint64_t endPos = host.getMaxNumNodes();
    if (pos >= endPos)
        return;
    uint64_t *nodeIndex = (uint64_t*)host.mapNodeIndex.ptr;
    while ( (nodeIndex[pos] == 0 || (nodeIndex[pos] & 0x8000000000000000ull)) && pos < endPos)
        pos+=1;            
}        

