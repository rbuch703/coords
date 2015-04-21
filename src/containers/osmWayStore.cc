
#include "osmWayStore.h"
#include <sys/mman.h>

LightweightWayStore::LightweightWayStore(const char* indexFileName, const char* dataFileName, bool optimizeForStreaming) {
    mapWayIndex = init_mmap(indexFileName, true, false);
    mapWayData  = init_mmap(dataFileName, true, true);

    if (optimizeForStreaming)
    {
        madvise( mapWayData.ptr, mapWayData.size, MADV_SEQUENTIAL);
        madvise( mapWayIndex.ptr, mapWayIndex.size, MADV_SEQUENTIAL);
    }

}

LightweightWayStore::LightweightWayStore(const std::string &baseName, bool optimizeForStreaming):
    LightweightWayStore((baseName + ".idx").c_str(), (baseName +".data").c_str(), optimizeForStreaming)
{}


OsmLightweightWay LightweightWayStore::operator[](uint64_t wayId)
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    assert(wayIndex[wayId] != 0 && "trying to access non-existent way");
    uint64_t wayPos = wayIndex[wayId];
    return OsmLightweightWay((uint8_t*)mapWayData.ptr + wayPos);
}

bool LightweightWayStore::exists(uint64_t wayId) const
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    if (wayId >= getMaxNumWays())
        return false;
        
    return wayIndex[wayId] != 0;
}

/*
void LightweightWayStore::syncRange(uint64_t lowWayId, uint64_t highWayId) const
{
    uint64_t *wayIndex = (uint64_t*)mapWayIndex.ptr;
    while (!wayIndex[lowWayId] && lowWayId < getMaxNumWays()) lowWayId++;
    while (!wayIndex[highWayId] && highWayId < getMaxNumWays()) highWayId++;

    if (lowWayId > getMaxNumWays() ) return;
    if (highWayId > getMaxNumWays())
        highWayId = getMaxNumWays();
    if (lowWayId >= highWayId) return;
    
    uint64_t lowPos = wayIndex[lowWayId];
    uint64_t highPos= wayIndex[highWayId];
    
    
    int res = sync_file_range(mapWayData.fd, lowPos, highPos - lowPos, 
                              SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER);
    if (res != 0)
        perror("sync_file_range");

    assert(highPos > lowPos);
    //cout << "syncing file range " << lowPos << " -> " << highPos << endl;

    uint64_t addr = ((uint64_t)mapWayData.ptr) + lowPos;
    size_t pageSize = sysconf (_SC_PAGESIZE);

    addr = addr / pageSize * pageSize;

    res = madvise(  (void*)addr, highPos - lowPos, MADV_DONTNEED);
    if (res != 0)
        perror("madvise");
} */


LightweightWayStore::LightweightWayIterator LightweightWayStore::begin() 
{ 
    return LightweightWayIterator(*this, 0);
}

LightweightWayStore::LightweightWayIterator LightweightWayStore::end()   
{ 
    return LightweightWayIterator(*this, getMaxNumWays());
}

uint64_t LightweightWayStore::getMaxNumWays() const 
{ 
    return mapWayIndex.size / sizeof(uint64_t); 
}


LightweightWayStore::LightweightWayIterator::LightweightWayIterator( LightweightWayStore &host, uint64_t pos):
    host(host), pos(pos) 
{
    advanceToNextWay();
}

LightweightWayStore::LightweightWayIterator& LightweightWayStore::LightweightWayIterator::operator++() {
    pos++;
    advanceToNextWay();
    return *this;
}

OsmLightweightWay LightweightWayStore::LightweightWayIterator::operator *() {
    return host[pos];
}

bool LightweightWayStore::LightweightWayIterator::operator !=( LightweightWayIterator &other) const
{ 
    return pos != other.pos;
} 

void LightweightWayStore::LightweightWayIterator::advanceToNextWay() {
    uint64_t endPos = host.getMaxNumWays();
    if (pos >= endPos)
        return;
    uint64_t *wayIndex = (uint64_t*)host.mapWayIndex.ptr;
    while (wayIndex[pos] == 0 && pos < endPos)
        pos+=1;            
}        

