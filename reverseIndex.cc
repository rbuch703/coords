
#include "reverseIndex.h"

#include <string.h> //for memcpy
#include <assert.h>
#include <iostream>

#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); assert(false && errMsg); exit(EXIT_FAILURE);}}

const uint32_t IS_WAY_REFERENCE = 0x80000000;

using namespace std;

/* on-disk and in-memory layout for RefList:
    - 1x uint32_t numEntries: number of actually present entries
    - 1x uint32_t maxNumEntries: number of entries that can be present without requiring a resize;
    - 'numEntries' x uint32_t entries: the actual references. References from 
      ways have their IS_WAY_REFERENCE bit set; references without that bit
      set are relation references 
*/


RefList::RefList( mmap_t* src, uint64_t srcOffset) : src(src), srcOffset(srcOffset) {};

RefList RefList::initialize(mmap_t *src, uint64_t srcOffset, uint32_t maxNumEntries)
{
    uint32_t *basePtr = (uint32_t*)src->ptr + srcOffset;
    basePtr[0] = 0; //numEntries;
    basePtr[1] = maxNumEntries;
    return RefList(src, srcOffset);
}

void RefList::resize(uint64_t newOffset, uint32_t newMaxNumEntries)
{
    MUST( newMaxNumEntries >= this->getNumEntries(), "new container cannot hold all elements");
    
    uint32_t *newBasePtr = (uint32_t*)src->ptr + newOffset;
    
    newBasePtr[0] = this->getNumEntries();
    newBasePtr[1] = newMaxNumEntries;
    memcpy(newBasePtr + 2, this->getBasePtr() + 2, this->getNumEntries() * sizeof(uint32_t));    

    
    this->getBasePtr()[0] = 0;  //invalidate the old entry
    this->srcOffset = newOffset;
}

/* either adds the reference (and returns zero), or returns the number of
   refs this entry would need to be able to hold in order to add the 
   reference. */
int RefList::add(uint32_t ref)
{
    uint32_t &numEntries = this->getBasePtr()[0];
    
    for (uint64_t i = 0; i < numEntries; i++)
    {
        if (getBasePtr()[2+i] == ref)
            return 0; //reference already exists; won't add a duplicate
    }
    
    if ( numEntries == this->getMaxNumEntries() )
        return numEntries + 1;
        
    getBasePtr()[2 + numEntries++] = ref;
    return 0;
}

uint32_t RefList::getNumEntries() const    { return getBasePtr()[0]; }
uint32_t RefList::getMaxNumEntries() const { return getBasePtr()[1]; }
uint64_t RefList::getOffset() const        { return srcOffset; }


      uint32_t* RefList::getBasePtr()       { return (uint32_t*)src->ptr + srcOffset;}
const uint32_t* RefList::getBasePtr() const { return (uint32_t*)src->ptr + srcOffset;}

ReverseIndex::ReverseIndex(const char* indexFileName, const char* auxFileName) { 
    index = init_mmap(indexFileName, true, true);
    auxIndex= init_mmap( auxFileName, true, true);
    
    /* conservative guess: without a dedicated way of determining where 
       the last auxIndex entry ends, assume that it ends at the end of the
       underlying file.
       This assumption errs on the side of caution: The true ending of 
       the data may be well before that (since the file size is a multiple
       of the page size irrespective of the actual data it contains), but 
       never after.
        */
    MUST( auxIndex.size % sizeof(uint32_t) == 0, "page size not multiple of uint32_t size");
    nextRefListOffset =  (auxIndex.size / sizeof(uint32_t));
}
    
ReverseIndex::~ReverseIndex() {
    free_mmap(&index);
    free_mmap(&auxIndex);
}
    
bool ReverseIndex::isReferenced( uint64_t id) const {
    uint32_t* idx = (uint32_t*) index.ptr;
    
    uint32_t val = idx[id];
    if (val == 0)
        return false;
        
    if (val & IS_WAY_REFERENCE)
    {
        val &= ~IS_WAY_REFERENCE; //remove is-way-reference indicator bit from wayId
        cout << "entry " << id << " is referenced by way " << val << endl;
        return true;
    }
    
    assert(false && "parsing of auxiliary files not implemented");
    exit(0);
}
    
uint64_t ReverseIndex::reserveSpaceForRefList(uint32_t numEntries) {
    //cout << "creating new ref list of size " << numEntries << endl;
    uint64_t numUints = (numEntries + 2);  //see on-disk layout above
    uint64_t refListOffset = nextRefListOffset;
    nextRefListOffset += numUints;

    ensure_mmap_size( &auxIndex, nextRefListOffset * sizeof(uint32_t) );
    return refListOffset;
}
    
void ReverseIndex::growRefList(RefList &refList, uint32_t minNumEntries)
{
    MUST( minNumEntries > refList.getMaxNumEntries(), "shrinking ref lists is not supported");
    /* add 50% spare space to allow for adding more entries without having 
       to allocate new space yet again */
    uint32_t nEntries = minNumEntries *3 / 2;
    refList.resize( reserveSpaceForRefList(nEntries), nEntries);
} 

/*    uint64_t getAuxFileIndex(uint32_t* ptr)
    {
        return (ptr - (uint32_t*)auxIndex.ptr);
    }
  */  
void ReverseIndex::addReferenceFromWay(uint64_t targetId, uint64_t wayId)
{
    MUST(wayId < (((uint64_t)1) << 31), "way IDs bigger than or equal 2³¹ are currently unsupported");
    ensure_mmap_size( &index, (targetId+1) * sizeof(uint32_t));
    uint32_t *pos =(uint32_t*)index.ptr;
    if (pos[targetId] == 0) //no references yet
    {
        pos[targetId] = wayId | IS_WAY_REFERENCE;
        //cout << "added (wayId=" << wayId << ") as only reference to entry " << targetId << endl;
        return;
    }
    
    if (pos[targetId] & IS_WAY_REFERENCE) //only a single reference from a way exists 
    {
        if (pos[targetId] == (wayId | IS_WAY_REFERENCE) )
        {
            //cout << "[WARN] reference of way " << wayId << " to entry " << targetId << " already registered. Won't add a duplicate." << endl;
            return;
        }
        
        /* Need to store two way references (the one already present and 
         * the newly added one). So we would have to allocate space in the
         * auxiliary index for both entries, which is not yet implemented*/
        uint32_t storedWayId = pos[targetId];
        RefList refList = RefList::initialize(&auxIndex, reserveSpaceForRefList(2), 2); //needs to hold two references
        MUST( 0 == refList.add(storedWayId), "AuxiliaryIndexEntry data corruption");
        MUST( 0 == refList.add(wayId | IS_WAY_REFERENCE), "AuxiliaryIndexEntry data corruption");
        pos[targetId] = refList.getOffset();
                    
//            cout << "creating ref list to hold additional (wayId=" << wayId << ") for entry " << targetId << endl;
    }
    
    //cout << "adding ref (wayId=" << wayId << ") to entry " << targetId << endl;
    RefList refList( &auxIndex, pos[targetId]);
    uint32_t res = refList.add( wayId | IS_WAY_REFERENCE);
    if (res == 0)
        return;
    
    //cout << "resizing refs list for entry " << targetId << " to size " << res << endl;        
    growRefList(refList, res);
    
    /* growRefList() changes the location of the ref list, so we have to update the
     * corresponding index entry.
     */
    pos[targetId] = refList.getOffset();
    MUST( 0 == refList.add( wayId | IS_WAY_REFERENCE), "RefList data corruption");
}
