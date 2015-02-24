
#ifndef SERIALIZABLE_MAP_H
#define SERIALIZABLE_MAP_H

#include <map>
#include "persistentVector.h"

template <typename KEY_T, typename VALUE_T, int MAX_ITEMS_IN_MEMORY>
class SerializableMap
{
public:
    SerializableMap(const char* filename, bool clearContents = false): filename(filename) {
        onDiskMap = new PersistentVector< std::pair<KEY_T, VALUE_T> >(filename, clearContents);
    }
    
    ~SerializableMap() {
        merge(); //to bring all map contents to disk
        delete onDiskMap;
    }
    
    VALUE_T getHighestValue() const
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
    void merge() {
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
    
    void insert(const std::pair<KEY_T, VALUE_T> &kv)
    {
        assert( this->count(kv.first) == 0);
        inMemoryMap.insert(kv);
        if (inMemoryMap.size() >= MAX_ITEMS_IN_MEMORY)
            this->merge();
    }
    
    int count(KEY_T key) {
       return inMemoryMap.count(key) || (bool)onDiskPosition(key);
    }
    
    uint64_t size() const { return inMemoryMap.size() + onDiskMap->size();}
    
    VALUE_T& operator[](const KEY_T &key) {
        assert( count(key) );
        return inMemoryMap.count(key) ?
            inMemoryMap[key] :
            *onDiskPosition(key);
    }
    
private:
    VALUE_T* onDiskPosition(KEY_T key) {
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

private:
    std::string filename;
    std::map<KEY_T, VALUE_T> inMemoryMap;
    PersistentVector< std::pair<KEY_T, VALUE_T> > *onDiskMap;
//    mmap_t                   onDiskMap;
};


#endif
