#ifndef REVERSE_INDEX_H
#define REVERSE_INDEX_H

#include "mem_map.h"

class RefList {

public:
    RefList( mmap_t* src, uint64_t srcOffset);
    static RefList initialize(mmap_t *src, uint64_t srcOffset, uint32_t maxNumEntries);
    void resize(uint64_t newOffset, uint32_t newMaxNumEntries);
    int add(uint32_t ref);

    uint32_t getNumEntries() const;
    uint32_t getMaxNumEntries() const;
    uint64_t getOffset() const;
    
private:
    uint32_t* getBasePtr();
    const uint32_t* getBasePtr() const;

    mmap_t *src;
    uint64_t srcOffset;
};


class ReverseIndex {
public:
    ReverseIndex(const char* indexFileName, const char* auxFileName);
    ~ReverseIndex();
    bool isReferenced( uint64_t id) const;
    void addReferenceFromWay(uint64_t targetId, uint64_t wayId);

private:
    uint64_t reserveSpaceForRefList(uint32_t numEntries);
    void growRefList(RefList &refList, uint32_t minNumEntries);

private:
     mmap_t index;
     mmap_t auxIndex;
     uint64_t nextRefListOffset;
};

#endif
