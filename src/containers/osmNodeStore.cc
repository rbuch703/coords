

#include "containers/osmNodeStore.h"

NodeStore::NodeStore(const char* indexFileName, const char* dataFileName) {
    mapIndex = init_mmap(indexFileName, true, false);
    mapData  = init_mmap(dataFileName, true, true);
}

NodeStore::NodeStore(std::string baseName): 
    NodeStore( (baseName + ".idx").c_str(), (baseName + ".data").c_str())
{ }


OsmNode NodeStore::operator[](uint64_t nodeId) const
{
    uint64_t *index = (uint64_t*)mapIndex.ptr;
    assert(index[nodeId] != 0 && "trying to access non-existent node");
    uint64_t dataOffset = index[nodeId];
    return OsmNode((uint8_t*)mapData.ptr + dataOffset);

}

bool NodeStore::exists(uint64_t nodeId) const
{
    uint64_t *index = (uint64_t*)mapIndex.ptr;
    if (nodeId >= getMaxNumNodes())
        return false;
        
    return index[nodeId] != 0;
}

NodeStore::NodeIterator NodeStore::begin() 
{ 
    return NodeIterator(*this, 0);
}

NodeStore::NodeIterator NodeStore::end()   
{ 
    return NodeIterator(*this, getMaxNumNodes());
}

uint64_t NodeStore::getMaxNumNodes() const 
{ 
    return mapIndex.size / sizeof(uint64_t); 
}


NodeStore::NodeIterator::NodeIterator( NodeStore &host, uint64_t pos): host(host), pos(pos) 
{
    advanceToNextNode();
}

NodeStore::NodeIterator& NodeStore::NodeIterator::operator++() 
{
    pos++;
    advanceToNextNode();
    return *this;
}

OsmNode NodeStore::NodeIterator::operator *() 
{
    return host[pos];
}

bool NodeStore::NodeIterator::operator !=( NodeIterator &other) const
{ 
    return pos != other.pos;
} 


void NodeStore::NodeIterator::advanceToNextNode() {
    uint64_t endPos = host.getMaxNumNodes();
    uint64_t *index = (uint64_t*)host.mapIndex.ptr;
    while (pos < endPos && index[pos] == 0)
        pos+=1;            
}        


