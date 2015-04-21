
#include "osmRelationStore.h"

//======================================================

RelationStore::RelationStore(const char* indexFileName, const char* dataFileName) {
    mapRelationIndex = init_mmap(indexFileName, true, false);
    mapRelationData  = init_mmap(dataFileName, true, true);
}

RelationStore::RelationStore(std::string baseName): 
    RelationStore( (baseName + ".idx").c_str(), (baseName + ".data").c_str())
{ }


OsmRelation RelationStore::operator[](uint64_t relationId) const
{
    uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
    assert(relationIndex[relationId] != 0 && "trying to access non-existent relation");
    uint64_t dataOffset = relationIndex[relationId];
    return OsmRelation((uint8_t*)mapRelationData.ptr + dataOffset);

}

bool RelationStore::exists(uint64_t relationId) const
{
    uint64_t *relationIndex = (uint64_t*)mapRelationIndex.ptr;
    if (relationId >= getMaxNumRelations())
        return false;
        
    return relationIndex[relationId] != 0;
}

RelationStore::RelationIterator RelationStore::begin() 
{ 
    return RelationIterator(*this, 0);
}

RelationStore::RelationIterator RelationStore::end()   
{ 
    return RelationIterator(*this, getMaxNumRelations());
}

uint64_t RelationStore::getMaxNumRelations() const 
{ 
    return mapRelationIndex.size / sizeof(uint64_t); 
}


RelationStore::RelationIterator::RelationIterator( RelationStore &host, uint64_t pos): host(host), pos(pos) 
{
    advanceToNextRelation();
}

RelationStore::RelationIterator& RelationStore::RelationIterator::operator++() 
{
    pos++;
    advanceToNextRelation();
    return *this;
}

OsmRelation RelationStore::RelationIterator::operator *() 
{
    return host[pos];
}

bool RelationStore::RelationIterator::operator !=( RelationIterator &other) const
{ 
    return pos != other.pos;
} 


void RelationStore::RelationIterator::advanceToNextRelation() {
    uint64_t endPos = host.getMaxNumRelations();
    uint64_t *relationIndex = (uint64_t*)host.mapRelationIndex.ptr;
    while (pos < endPos && relationIndex[pos] == 0)
        pos+=1;            
}        
