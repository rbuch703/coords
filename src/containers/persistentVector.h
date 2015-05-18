
#ifndef PERSISTENT_VECTOR_H
#define PERSISTENT_VECTOR_H

#include "misc/mem_map.h"
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

template<typename T> class PersistentVector {
public:
    PersistentVector(const char* filename, bool clearContents);
    ~PersistentVector();
        
    void push_back(const T &val);
    void pop_back ();
    void resize(uint64_t newSize);
    void clear();

    uint64_t size();
    
    T& operator[](uint64_t idx);

    T* begin();
    T* begin() const;
    T* end();
    T* end()   const;

private:
    mmap_t memMap;
};

// =========================================

template <typename T>
PersistentVector<T>::PersistentVector(const char* filename, bool clearContents) {
    memMap = init_mmap(filename, true, true, clearContents);
    if (memMap.size < 8)    //not yet initialized
    {
        ensure_mmap_size(&memMap, 8);
        *((uint64_t*)memMap.ptr) = 0;   //set item count
    }
}

template <typename T>
PersistentVector<T>::~PersistentVector()
{
    free_mmap(&memMap);
}

template <typename T>
void PersistentVector<T>::push_back(const T &val)
{
    uint64_t necessarySize = sizeof(uint64_t) + (size()+1) * sizeof(T);
    if (memMap.size < necessarySize)
        ensure_mmap_size(&memMap, necessarySize);
        
    T* dest = (T*)(  ((uint8_t*)memMap.ptr) + sizeof(uint64_t) + size() * sizeof(T));
    *dest = val;
        
    (*(uint64_t*)memMap.ptr) += 1;
}

template <typename T>
void PersistentVector<T>::pop_back() 
{
    if (size() > 0)
        setSize(size() - 1);
}

template <typename T>
T& PersistentVector<T>::operator[](uint64_t idx)
{
    assert(idx < size() && "Array index out of bounds.");
    return *(T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t) + idx * sizeof(T));
}

template <typename T>
void PersistentVector<T>::resize(uint64_t newSize) 
{
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

template <typename T>
void PersistentVector<T>::clear() 
{ 
    *(uint64_t*)memMap.ptr = 0;
}

template <typename T>
uint64_t PersistentVector<T>::size() 
{ 
    return *(uint64_t*)memMap.ptr;  
}

template <typename T>
T* PersistentVector<T>::begin()       
{ 
    return (T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t));
}

template <typename T>
T* PersistentVector<T>::begin() const 
{ 
    return (const T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t));
}

template <typename T>
T* PersistentVector<T>::end()
{ 
    return (T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t) + size() * sizeof(T));
}

template <typename T>
T* PersistentVector<T>::end() const 
{ 
    return (const T*) (((uint8_t*)memMap.ptr) + sizeof(uint64_t) + size() * sizeof(T));
}



#endif
