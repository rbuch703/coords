
#include "containers/chunkedFile.h"
#include <assert.h>
#include <iostream>

using std::string;

const uint64_t ChunkedFile::chunkSizes[] = 
    {0, 17, 18, 24, 38, 51, 56, 64, 73, 87, 101, 107, 119, 133, 144, 154, 167, 183, 200, 218, 237,
     259, 278, 306, 333, 365, 403, 442, 492, 547, 605, 674, 750, 838, 941, 1056, 1187, 1335, 1514,
     1719, 1960, 2247, 2587, 2990, 3474, 4066, 4792, 5704, 6835, 8203, 10075, 12502, 15691, 19712,
     25246, 32260, 131072, 524288, 2097152, 8388608, 33554432, 134217728, 536870912, 2147483648};


const uint64_t ChunkedFile::numChunkSizes =
    sizeof(chunkSizes) / sizeof(uint64_t);


ChunkedFile::ChunkedFile(string filename): filename(filename)
{
    /* we use a uint8_t as the chunk marker. Of its 8 bits, one is used
     * to mark the chunk as 'used' or 'free', and another one is used to 
     * mark the chunk as valid. This leaves 6 bits for the chunk size,
     * meaning we can have at most 2^6 = 64 different chunk sizes. */
    MUST(numChunkSizes <= 64, "invalid number of different chunk sizes");
    fileMap = init_mmap(filename.c_str(), true, true);
    freeLists = new std::vector<uint64_t>[ChunkedFile::numChunkSizes];

    if (fileMap.size == 0) 
    //newly created --> needs to be initialized, ignore free-list (if it exists)
    {
        //needs to be able to hold at least the free space counter
        ensure_mmap_size(&fileMap, sizeof(uint64_t));
        this->setFreeSpaceAtEnd( fileMap.size - sizeof(uint64_t));
    } else
    {
        loadFreeLists();
    }
}

void ChunkedFile::loadFreeLists()
{
    FILE* f = fopen( (filename+".free").c_str(), "rb");
    if (!f)
        return;
        
    fseek(f, 0, SEEK_END);
    uint64_t fileSize = ftell(f);
    rewind(f);
    
    MUST(fileSize % sizeof(uint64_t) == 0, "corrupted free chunk file");
    uint64_t numEntries = fileSize / sizeof(uint64_t);
    while (numEntries--)
    {
        uint64_t entry = 0;
        if (1 != fread(&entry, sizeof(entry), 1, f))
        {
            std::cout << "[WARN] read error while parsing 'free' list, skipping." << std::endl;
            continue;
        }
            
        uint64_t chunkSize = entry >> 56;
        uint64_t chunkPos  = entry & 0x00FFFFFFFFFFFFFFull;
        if (chunkSize & INVALID_CHUNK)
        {
            std::cout << "[WARN] invalid entry in 'free' list, skipping." << std::endl;
            continue;
        }
        
        if (chunkSize > CHUNK_SIZE_MASK)
        {
            std::cout << "[WARN] invalid entry in 'free' list, skipping." << std::endl;
            continue;
        }
        
        freeLists[chunkSize].push_back(chunkPos);

    }
    fclose(f);
}

ChunkedFile::~ChunkedFile()
{
    MUST(numChunkSizes <= CHUNK_SIZE_MASK+1, "number of chunk sizes bigger than 64");
    FILE *f = fopen( (filename+".free").c_str(), "wb");
    for (uint64_t i = 0; i < numChunkSizes; i++)
    {
        for (uint64_t freeChunk : freeLists[i])
        {
            MUST( !(freeChunk & 0xFF00000000000000ull), "invalid chunk address (<= 2^56)");
            uint64_t entry = (i << 56) | freeChunk;
            fwrite(&entry, sizeof(entry), 1, f);
        }   
    }
    fclose(f);
    
    delete [] freeLists;

    free_mmap(&fileMap);

}


uint64_t ChunkedFile::getFreeSpaceAtEnd() const
{
    uint64_t res = *((uint64_t*)fileMap.ptr);
    assert(res <= fileMap.size);
    return res;
}

void ChunkedFile::setFreeSpaceAtEnd(uint64_t freeSpace)
{
    *((uint64_t*)fileMap.ptr) = freeSpace;
}

void ChunkedFile::increaseSize(uint64_t additionalSpace)
{
    uint64_t oldFreeSpace = this->getFreeSpaceAtEnd();
    uint64_t oldFileSize  = this->fileMap.size;
    
    ensure_mmap_size(&fileMap, fileMap.size + additionalSpace);
    int64_t sizeDifference = fileMap.size - oldFileSize;
    assert(sizeDifference >= 0);
    setFreeSpaceAtEnd( oldFreeSpace + sizeDifference);
}

Chunk ChunkedFile::createChunk(uint64_t chunkSize)
{
    uint64_t contentSize = chunkSize;
    chunkSize += 1; //need space for the 1 byte chunk size marker
/*
    static FILE* f = fopen("chunkSizes.bin", "wb");
    uint32_t cs = chunkSize;
    fwrite(&cs, sizeof(cs), 1, f);
*/
    //std::cerr << chunkSize << std::endl;
    MUST( chunkSize < chunkSizes[numChunkSizes-1], "chunk size > 1.6GB requested");

    int32_t chunkSizeIdx = -1;
    for (uint64_t i = 0; i < ChunkedFile::numChunkSizes; i++)
        if (chunkSize <= chunkSizes[i])
        {
            chunkSizeIdx = i;
            chunkSize = chunkSizes[i];
            break;
        }
    MUST(chunkSizeIdx > -1, "unsupported chunk size requested");

    uint64_t chunkPos = 0;
    MUST( (uint64_t)chunkSizeIdx < numChunkSizes, "chunk size > 1.6GB requested. This is unsupported");

    if (freeLists[chunkSizeIdx].size() > 0)
    {
        //if an unused chunk of the given size exists, recycle it
        chunkPos = freeLists[chunkSizeIdx].back();
        freeLists[chunkSizeIdx].pop_back();
    } else
    {
        /* Otherwise create a new chunk at the end of the data area
         * (increasing the file size if necessary) */
        if (this->getFreeSpaceAtEnd() <= (uint64_t)chunkSize)
            this->increaseSize( chunkSize - this->getFreeSpaceAtEnd() );
        
        assert( this->getFreeSpaceAtEnd() >= (uint64_t)chunkSize );

        chunkPos = this->getStartPosOfFreeSpace();
        this->setFreeSpaceAtEnd( getFreeSpaceAtEnd() - chunkSize);
    }
    uint8_t* chunk = (uint8_t*)fileMap.ptr + chunkPos;
    //initialize chunk by setting its size marker
    chunk[0] = chunkSizeIdx;

    #ifndef NDEBUG
        //mark internal slack space for easier debugging
        for (uint64_t i = contentSize+1; i < chunkSize; i++)
            chunk[i] = 0xFF;
    #endif    
    
    //the space relevant to the user start just after the 1byte marker
    return Chunk( chunk + 1, chunkPos + 1, contentSize); 
}

void ChunkedFile::freeChunk(Chunk &chunk)
{
    MUST( chunk.dataPtr > fileMap.ptr, "invalid chunk");

    /*each chunk is preceeded by a one byte size marker that is transparent to the user
      but is still part of the file chunk and needs to be accounted for. */
    uint64_t chunkPos = chunk.getPositionInFile() - 1;

    MUST(chunkPos < fileMap.size, "invalid chunk");

    // 8 bytes freeSpaceCounter | 1 byte size marker of chunk '1' | chunk 1 | ...
    MUST(chunkPos >= 8, "invalid chunk position");

    uint8_t chunkSize = ((uint8_t*)fileMap.ptr)[chunkPos];

    if (chunkSize & (INVALID_CHUNK | UNUSED_CHUNK))
    {
        std::cout << "[WARN] attempt to free invalid or unused chunk, skipping." << std::endl;
        return;
    }
    chunkSize &= CHUNK_SIZE_MASK; //mask out "unused" and "invalid" bits
    if (chunkSize >= numChunkSizes)
    {
        std::cout << "[WARN] attempt to free chunk of invalid size, skipping." << std::endl;
        return;
    }
    
    ((uint8_t*)fileMap.ptr)[chunkPos] |=  UNUSED_CHUNK;
    freeLists[chunkSize].push_back(chunkPos);
    
}

/*
uint8_t* ChunkedFile::getChunkDataPtr(uint64_t filePos)
{
    MUST( filePos <= fileMap.size, "invalid file position");
    return ((uint8_t*)fileMap.ptr) + filePos;
}*/


uint64_t ChunkedFile::getStartPosOfFreeSpace() const
{
    // File layout: 8byte free space size counter|<data>|<free space at end>| 
    return fileMap.size - this->getFreeSpaceAtEnd();
}

bool ChunkedFile::isValidChunk(uint64_t pos) const
{
    if (pos >= fileMap.size) return false;
    uint8_t sizeMarker = ((uint8_t*)fileMap.ptr)[pos];
    if (sizeMarker & INVALID_CHUNK) return false;
    sizeMarker &= CHUNK_SIZE_MASK; //remove "valid" bit and "unused" bit
    if (sizeMarker >= numChunkSizes) return false;
    uint64_t size = ChunkedFile::chunkSizes[sizeMarker];
    return pos + size <= (fileMap.size - getFreeSpaceAtEnd())  ;
}


ChunkedFile::Iterator::Iterator(
    const ChunkedFile& host, uint8_t *chunkPtr, bool isBeyond):
    host(host), chunkPtr(chunkPtr)
{
    //std::cout << "initialized at " << (this->chunkPtr - (uint8_t*)host.fileMap.ptr) << std::endl;
    /* points directly beyond the last valid entry (e.g. is the result of end()),
     * so do not attempt to dereference anything */
    if (isBeyond)
        return;
    
    uint8_t chunkHeader = *this->chunkPtr;
    if ( chunkHeader & UNUSED_CHUNK)
        this->moveToNextValidChunk();
        
    //std::cout << "header is " << (int)chunkHeader << std::endl;
}

ChunkedFile::Iterator& ChunkedFile::Iterator::operator++()
{
    this->moveToNextValidChunk();
    return *this;
}

bool ChunkedFile::Iterator::operator!=(const ChunkedFile::Iterator& other) const
{
    return chunkPtr != other.chunkPtr;
}

void ChunkedFile::Iterator::moveToNextValidChunk()
{
    uint8_t *beyond = ((uint8_t*)host.fileMap.ptr) + host.getStartPosOfFreeSpace();
    
    uint8_t chunkHeader = *this->chunkPtr;
    MUST( (chunkHeader & INVALID_CHUNK) == 0, "invalid chunk");
    uint64_t chunkSize = ChunkedFile::chunkSizes[ chunkHeader & CHUNK_SIZE_MASK];
    
    while (this->chunkPtr < beyond)
    {
        this->chunkPtr += chunkSize;

        chunkHeader = *this->chunkPtr;
        if (! (chunkHeader & (INVALID_CHUNK | UNUSED_CHUNK)))
        {
            //std::cout << "is now at " << (this->chunkPtr - (uint8_t*)host.fileMap.ptr) 
            //          << std::endl;
            return;
        }
        
        chunkSize = ChunkedFile::chunkSizes[ chunkHeader & CHUNK_SIZE_MASK];
            
    }
    MUST( this->chunkPtr == beyond, "overflow");
}

ChunkedFile::Iterator ChunkedFile::begin()
{
    //first chunk starts right after the uint64_t free space counter
    return Iterator(*this, ((uint8_t*)this->fileMap.ptr)+sizeof(uint64_t));
}

ChunkedFile::Iterator ChunkedFile::end()
{
    return Iterator(*this, ((uint8_t*)this->fileMap.ptr)+this->getStartPosOfFreeSpace(), true);
}


Chunk ChunkedFile::Iterator::operator*() const
{
    uint8_t chunkHeader = *this->chunkPtr;
    MUST( (chunkHeader & INVALID_CHUNK) == 0, "invalid chunk");
    uint64_t chunkSize = ChunkedFile::chunkSizes[ chunkHeader & CHUNK_SIZE_MASK];
    return Chunk(this->chunkPtr+1, this->chunkPtr - (uint8_t*)host.fileMap.ptr + 1, chunkSize-1);
}

