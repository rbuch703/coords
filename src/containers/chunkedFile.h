
#ifndef CHUNKED_FILE_H
#define CHUNKED_FILE_H

#include <stdint.h>

#include <vector>
#include <map>
#include <string>

#include <assert.h>
#include <string.h> //for memcpy

#include "misc/mem_map.h"
#include "config.h"

/* The class "ChunkedFile" represents a file containing data chunks of variable sizes that can 
   be created, read, updated and deleted without having to understand the contents of 
   individual chunks.
   It is intended for uses where chunk deletions and recreations are quite common. In these
   cases, allowing arbitrary chunk sizes would lead to wasted space whenever a chunk is deleted,
   as its space cannot easily be re-used for chunks of a different size. 
   Instead, this class only allows certain chunk sizes that roughly follow a power series (
   the actual chunk sizes where determined based on actual serialized OSM entity sizes). 
   When creation of a new chunk (of arbitrary size) is 
   requested, the request is fulfilled by serving a new chunk of the closest size
   from the power series that is at least as big as requested. When a chunk is deleted,
   its actual chunk size (which may have been bigger than requested) is reclaimed,
   and added to a 'free list'. Since the power series has only 63 distinct entries (up
   to a maximum chunk size of about 1,9GB), the free list simply consists of a list of 
   free chunks for each of the 63 different possible sizes. So free space management is
   simplified. 
*/
class Chunk;

class ChunkedFile 
{
public:
    ChunkedFile(std::string filename);
    ~ChunkedFile();
    //uint64_t createChunk(uint64_t size);
    Chunk createChunk(uint64_t size);
    void freeChunk(Chunk &chunk);

        
    class Iterator 
    {
    public:
        Iterator(const ChunkedFile& host, uint8_t *chunkPtr, bool isBeyond = false);
        Iterator& operator++();
        Chunk operator*() const;
        bool operator!=(const Iterator& other) const;
    private:
        void moveToNextValidChunk();
    private:
        const ChunkedFile &host;
        uint8_t *chunkPtr;
    };

    Iterator begin();
    Iterator end();

    //uint8_t* getChunkDataPtr(uint64_t filePos);
//private:
public: //'public' for debugging
    void increaseSize(uint64_t newSize);
    uint64_t getFreeSpaceAtEnd() const;
    void setFreeSpaceAtEnd(uint64_t freeSpace);
    uint64_t getStartPosOfFreeSpace() const;
    bool isValidChunk(uint64_t pos) const;

    static const uint64_t chunkSizes[];
    static const uint64_t numChunkSizes;

private:
    void loadFreeLists();
    enum CHUNK_BITS {INVALID_CHUNK = 0x40, UNUSED_CHUNK = 0x80, CHUNK_SIZE_MASK= 0x3F};

    std::string filename;
    std::vector<uint64_t> *freeLists;
    mmap_t fileMap;
};

class Chunk
{
public:
    Chunk(uint8_t *dataPtr, uint64_t filePos, uint64_t size): 
        dataPtr(dataPtr), filePos(filePos), currentIndex(0), size(size) {}
        
    uint64_t getSize() const { return size;}
    uint64_t getPositionInFile() const { return filePos;}

    template<typename T>
    T get() {
        MUST( currentIndex + sizeof(T) <= size, "invalid read in chunk"); 
        T *pos = (T*)(dataPtr + currentIndex);
        currentIndex += sizeof(T);
        return *pos;
    };

    template<typename T>
    void put(const T &value) {
        MUST( currentIndex + sizeof(T) <= size, "invalid write to chunk");
        *((T*)(dataPtr + currentIndex)) = value;
        currentIndex += sizeof(T);
    };
    
    void put(const void* src, uint64_t srcSize)
    {
        MUST( currentIndex + srcSize <= this->size, "invalid write to chunk")
        memcpy(dataPtr+currentIndex, src, srcSize);
        currentIndex += srcSize;
    }
    
    const uint8_t *getDataPtr() const {
        return dataPtr;
    }
    void resetPosition() {currentIndex = 0;}
private:
    uint8_t* dataPtr;
    uint64_t filePos;
    uint64_t currentIndex;
    uint64_t size;
    friend class ChunkedFile;
};

#endif

