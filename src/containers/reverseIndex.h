#ifndef REVERSE_INDEX_H
#define REVERSE_INDEX_H

#include "mem_map.h"
#include <string>

/* The class ReverseIndex represents a mapping of an OSM entity id (node, way or relation id) to a list
 * of OSM ways and relations that refer to that entity. This class is intended for the process of 
 * applying OSM updates ("replication diffs"), where changing one entity (e.g. a node) may implicitly
 * change other entities (e.g. changing a node implicitly changes a way that refers to that node). Thus,
 * one has to keep track of how changes to one entity implicitly change which other entities. The
 * ReverseIndex facilitates that book-keeping.
 *
 * A ReverseIndex is always backed by disk storage, so any changes to an ReverseIndex object will be persistent.
 *
 * A ReverseIndex does not distinguish between entity types for the entities it stores the reverse dependencies to.
 * So a practical application of this class should used three separate instances of this class to store the reverse
 * dependencies for all nodes, ways, and relation, respectively.
 * But a single ReverseIndex does distinguish between the types of reverse indices for each entity. So a single
 * ReverseIndex can be used to record all reverse dependencies to a given entity type (e.g. the reverse references 
 * to all nodes).
 *
 * On disk, the ReverseIndex consists of a mmap()'ed array of uint64_t, and an auxiliary file:
 * The entry Y at position X in the uint64_t array represents the reverse dependencies of the entity
 * with id X as follows:
 * - if Y is 0, there are no reverse dependencies for X
 * - if the most significant bit of Y is set, then X is referenced by exactly one way (and by no relation),
 *   and that way has the ID given by the lower 63 bits of Y
 * - if the most significant bit of Y is not set, then X is references by at least one relation or by multiple way
     (or a combination thereof). In this case, the value of Y is an offset into the auxiliary files, where the full
     list of reverse dependencies is recorded.
 * As most entities are referred to either not at all, or only by a single way, this storage scheme is fast and 
 * reduces memory consumption over alternative approaches: the reverse references to most entities consume only a
 * single uint64_t of memory.
 
 * A ReverseIndex assumes that all reference IDs (way and relation IDs) and all offsets into the auxiliary file
 * are at most 63 bits (not 64 bits!) wide, as the MSB of the stored uint64_t's is used as an independent flag.
 * Violating this assumpion will corrupt the ReverseIndex.
 */

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
