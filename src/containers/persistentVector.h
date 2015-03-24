
#ifndef PERSISTENT_VECTOR_H
#define PERSISTENT_VECTOR_H

#include "mem_map.h"
#include <assert.h>
/* The template class PersistentVector represents an vector that is
 * backed by a memory-mapped file. It acts mostly like the std::vector,
 * but its contents persist beyond program termination.
 *
 * Note: Do not store pointers in a PersistentVector !
 *       All data is stored exactly like its in-memory representation.
 *       Thus, storing pointers, which are not valid across multiple
 *       program executions, in a PersistentVector will lead to 
 *       memory corruption and program crashes
 *       
/*/

template<typename T>
class PersistentVector {
public:
    PersistentVector(const char* filename, bool clearContents) {
        memMap = init_mmap(filename, true, true, clearContents);
        if (memMap.size < 8)    //not yet initialized
        {
            ensure_mmap_size(&memMap, 8);
            *((uint64_t*)memMap.ptr) = 0;   //set item count
        }
    }
    
    void push_back(const T &val)
    {
        uint64_t necessarySize = sizeof(uint64_t) + (size()+1) * sizeof(T);
        if (memMap.size < necessarySize)
            ensure_mmap_size(&memMap, necessarySize);
            
        T* dest = (T*)(  ((uint8_t*)memMap.ptr) + sizeof(uint64_t) + size() * sizeof(T));
        *dest = val;
            
        (*(uint64_t*)memMap.ptr) += 1;
    }
    
    void pop_back() {
        if (size() > 0)
            setSize(size() - 1);
    }
    
    T& operator[](uint64_t idx)
    {
        assert(idx < size() && "Array index out of bounds.");
        return *(T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t) + idx * sizeof(T));
    }
    
    void resize(uint64_t newSize) {
        uint64_t oldSize = size();
        uint64_t necessarySize = sizeof(uint64_t) + newSize * sizeof(T);
        
        if (necessarySize > this->size())
            ensure_mmap_size(&memMap, necessarySize);
            
        T* base = (T*)(  ((uint8_t*)memMap.ptr) + sizeof(uint64_t));
        if (newSize > oldSize)
        {
            for (int i = oldSize; i < newSize; i++)
                base[i] = T();  //default constructor
        }
        *(uint64_t*)memMap.ptr = newSize;
    }
    
    void clear() { *(uint64_t*)memMap.ptr = 0;}
    uint64_t size() { return *(uint64_t*)memMap.ptr;  }

    T* begin()       { return (      T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t));}
    T* begin() const { return (const T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t));}
    T* end()         { return (      T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t) + size() * sizeof(T));}
    T* end()   const { return (const T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t) + size() * sizeof(T));}

private:
    //void setSize(uint64_t newSize) { *(uint64_t*)memMap.ptr = newSize; }

private:
    mmap_t memMap;
};

#endif
