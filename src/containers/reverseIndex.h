#ifndef REVERSE_INDEX_H
#define REVERSE_INDEX_H

#include "mem_map.h"
#include <string>

class RefList;

class ReverseIndex {
public:
    //ReverseIndex(const char* indexFileName, const char* auxFileName, bool removeOldContent = false);
    ReverseIndex(std::string baseFileName, bool removeOldContent = false);
    ReverseIndex(std::string indexFileName, std::string auxFileName, bool removeOldContent = false);
    ~ReverseIndex();
    bool isReferenced( uint64_t id);
    void addReferenceFromWay(uint64_t targetId, uint64_t wayId);
    void addReferenceFromRelation(uint64_t targetId, uint64_t relationId);

private:
    uint64_t reserveSpaceForRefList(uint64_t numEntries);
    void growRefList(RefList *refList, uint64_t minNumEntries);

private:
     mmap_t index;
     mmap_t auxIndex;
     uint64_t nextRefListOffset;
};

#endif
