
#ifndef SERIALIZABLE_MAP_H
#define SERIALIZABLE_MAP_H

#include <map>
#include "persistentVector.h"

/* The template class SerializableMap represents a dictionary (key-value store) that
 * is partially backed by on-disk storage, and can manually (via merge() or through
 * the destructor) be forced to be completely written to disk and thus made persistent.
 *
 * The indented use for SerializableMap are huge dictionaries (gigabytes or data) 
 * that at the same time should be persistent, stored compactly and fast. 
 * Here, storing the dictionary as a std::map would waste memory: a std::map entry
 * requires about 16 bytes of memory for book-keeping in addition to storing the
 * actual data, which can prohibatively increase memory consumption for huge dictionaries
 * where keys and values themselves require little memory.
 * Instead, a PersistentVector keeps newly created entries in memory, and - once 
 * a threshold is reached - write them to disk in bulk. As the on-disk part is memory-mapped,
 * it will be kept in memory as long as there is enough memory available (as decided by
 * the OS). And when not enough memory is available, the PersistentVector with transparently
 * degrade gracefully: the OS will remove some parts of the memory-mapped file from memory,
 * but these will transparently be loaded again when used.
 *
 * Internally, a SerializableMap consists of a std::map and a memory-mapped array of 
 * key-value pairs that is kept sorted by key. Newly added key-value pairs are stored
 * in the std::map. Whenever this std::map would contain more than MAX_ITEMS_IN_MEMORY,
 * its contents are written to disk using a merge step identical to that used in MergeSort.
 * This merge step creates a separate on-disk file containing the merged contents. After
 * the merge has completed, the new file atomically overwrites the old one. At this point,
 * the on-disk storage contains all key-value pairs, and the in-memory storage is cleared.
 *
 * Limitations:
 * - currently, there is no way to remove an entry from a serializable map.
 * - merges are not currently fsynced for performance reasons, so OS crashes or power outages can lead to data corruption
*/

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
class SerializableMap
{
public:
    SerializableMap(const char* filename, bool clearContents = false);    
    ~SerializableMap();
    
    void merge();
    void insert(const std::pair<KEY_T, VALUE_T> &kv);
    
    int      count(KEY_T key);
    uint64_t size() const;
    
    VALUE_T  getHighestValue() const;
    VALUE_T& operator[](const KEY_T &key);
    
private:
    VALUE_T* onDiskPosition(KEY_T key);

private:
    std::string filename;
    std::map<KEY_T, VALUE_T> inMemoryMap;
    PersistentVector< std::pair<KEY_T, VALUE_T> > *onDiskMap;
};




// ==========================================

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::SerializableMap(const char* filename, bool clearContents): filename(filename) {
    onDiskMap = new PersistentVector< std::pair<KEY_T, VALUE_T> >(filename, clearContents);
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::~SerializableMap() 
{
    merge(); //to bring all map contents to disk
    delete onDiskMap;
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
VALUE_T SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::getHighestValue() const
{
    VALUE_T val = VALUE_T();
    for ( const std::pair<KEY_T, VALUE_T> &kv: inMemoryMap)
        if (kv.second > val)
            val = kv.second;
            
    for ( const std::pair<KEY_T, VALUE_T> &kv: *onDiskMap)
        if (kv.second > val)
            val = kv.second;
    
    return val;    
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
void SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::merge() 
{
    /*FIXME: add separate code path for when the biggest key in the on-disk map
     *       is smaller than the smallest key in the in-memory map. In that case
     *       the in-memory map can simply be appended to the on-disk map instead
     *       of requiring a merger. This situation should be the norm for OSM ID
     *       remapping, where the entities are usually processed in ID order */
    PersistentVector< std::pair<KEY_T, VALUE_T> > *dest = 
        new PersistentVector< std::pair<KEY_T, VALUE_T> >( (std::string(filename)+".tmp").c_str(), true);
    
    std::pair<KEY_T, VALUE_T> *pos2 = onDiskMap->begin();
    
    for (const std::pair<KEY_T, VALUE_T> &val1 : inMemoryMap)
    {
        for ( ; pos2 != onDiskMap->end() && *pos2 < val1; pos2 ++)
            dest->push_back( *pos2);
            
        dest->push_back(val1);
    }
    
    for (; pos2 != onDiskMap->end(); pos2 ++)
        dest->push_back(*pos2);

    /* replace old file by new file (file descriptors to the previous file 
       name stay valid, so 'dest' is unaffected by the renaming of its 
       underlying file */
    rename( (filename+".tmp").c_str(), filename.c_str());
    delete onDiskMap;
    onDiskMap = dest;
    inMemoryMap.clear();
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
void SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::insert(const std::pair<KEY_T, VALUE_T> &kv)
{
    assert( this->count(kv.first) == 0);
    inMemoryMap.insert(kv);
    if (inMemoryMap.size() >= MAX_ITEMS_IN_MEMORY)
        this->merge();
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
int SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::count(KEY_T key) {
   return inMemoryMap.count(key) || (bool)onDiskPosition(key);
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
uint64_t SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::size() const 
{ 
    return inMemoryMap.size() + onDiskMap->size();
}

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
VALUE_T& SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::operator[](const KEY_T &key) 
{
    assert( count(key) );
    return inMemoryMap.count(key) ?
        inMemoryMap[key] :
        *onDiskPosition(key);
}


template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
VALUE_T* SerializableMap<KEY_T, VALUE_T, MAX_ITEMS_IN_MEMORY>::onDiskPosition(KEY_T key)
{
    if (onDiskMap->size() == 0) //empty --> cannot contain 'key'
        return NULL;
        
    uint64_t lo = 0;
    uint64_t hi = onDiskMap->size()-1;
    
    if (key < (*onDiskMap)[lo].first || key > (*onDiskMap)[hi].first)
        return NULL;
        
    while (lo + 1 < hi)
    {
        assert(lo <= hi);
        uint64_t mid = (lo + hi)/2;
        if ( (*onDiskMap)[mid].first == key)
            return &(*onDiskMap)[mid].second;
        else if ( (*onDiskMap)[mid].first < key)
            lo = mid;
        else 
            hi = mid;
    }
    
    return ((*onDiskMap)[lo].first == key ) ? &(*onDiskMap)[lo].second :
           ((*onDiskMap)[hi].first == key ) ? &(*onDiskMap)[hi].second :
             NULL;
}    


#endif
