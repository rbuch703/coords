

#include "rawTags.h"
#include "config.h"
#include <string.h> //for strlen

/* ON-DISK LAYOUT FOR TAGS:
    uint32_t numBytes
    uint16_t numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/


RawTags::RawTags(uint64_t numTags, uint64_t numTagBytes, 
                 const uint8_t *symbolicNameBits, 
                 const uint8_t* tagsStart):
    numTags(numTags), numTagBytes(numTagBytes),
    symbolicNameBits(symbolicNameBits), tagsStart(tagsStart) 
{}

RawTags::RawTags(const uint8_t* src)
{
    uint32_t totalNumBytes = *(uint32_t*)src;
    //std::cout << "\thas " << numTagBytes << "b of tags."<< std::endl;
    src += sizeof(uint32_t);
    
    uint16_t numTags = *(uint16_t*)src;
    src += sizeof(uint16_t);
    
    //uint8_t *symbolicNameBytes = pos;
    uint64_t numNames = numTags * 2; // key and value per tag
    uint64_t numSymbolicNameBytes = (numNames + 7) / 8;
    
    numTagBytes = totalNumBytes - numSymbolicNameBytes - sizeof(uint16_t);
    symbolicNameBits = src;
    tagsStart = src + numSymbolicNameBytes;
}

uint64_t RawTags::getSerializedSize(const TTags &tags)
{
    uint64_t numTags = tags.size();
    uint64_t numNames = numTags * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames +7) / 8; //one bit per name --> have to round up
    
    uint64_t numTagBytes = 0;
    for (const std::pair<std::string, std::string> &kv: tags)
    {
        numTagBytes += symbolicNameId.count(kv.first)  ? 1 : kv.first.length()  + 1;
        numTagBytes += symbolicNameId.count(kv.second) ? 1 : kv.second.length() + 1;
    }
    
    return sizeof(uint16_t) + 
           bitfieldSize * sizeof(uint8_t) +
           numTagBytes;

}

void RawTags::serialize(const TTags &tags, FILE* fOut)
{
    uint64_t tmp = getSerializedSize(tags);
    MUST(tmp < (1ull)<<32, "tag set size overflow");
    uint32_t numBytes = tmp;
    MUST(fwrite( &numBytes, sizeof(numBytes), 1, fOut) == 1, "write error");
    int64_t filePos = ftell(fOut);

    MUST( tags.size() < 1<<16, "attribute count overflow");
    uint16_t numTags = tags.size();
    MUST(fwrite( &numTags,  sizeof(numTags),  1, fOut) == 1, "write error");

    uint64_t numNames = numTags * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames +7) / 8; //one bit per name --> have to round up
    uint8_t *isSymbolicName = new uint8_t[bitfieldSize];
    memset(isSymbolicName, 0, bitfieldSize);
    
    int idx = 0;
    for ( const std::pair<std::string, std::string> &kv : tags)
    {
        int byteIdx = idx / 8;
        int bitIdx  = 7 - (idx % 8);
        if (symbolicNameId.count(kv.first))
            isSymbolicName[byteIdx] |= (1 << bitIdx);
        
        MUST( bitIdx > 0, "logic error");
        bitIdx -= 1;

        if (symbolicNameId.count(kv.second))
            isSymbolicName[byteIdx] |= (1 << bitIdx);
        
        idx += 2;
    }
    
    if (bitfieldSize)
    {
        MUST( fwrite( isSymbolicName, bitfieldSize, 1, fOut) == 1, "write error");
    }
    
    for (const std::pair<std::string, std::string> &kv : tags)
    {
        if (symbolicNameId.count(kv.first))
        {
            uint8_t symbolicId = symbolicNameId.at(kv.first);
            MUST( fwrite( &symbolicId, sizeof(symbolicId), 1, fOut) == 1, "write error");
        } else
        {
            const char* key = kv.first.c_str();
            MUST( fwrite( key, strlen(key)+1, 1, fOut) == 1, "write error");
        }
        
        if (symbolicNameId.count(kv.second))
        {
            uint8_t symbolicId = symbolicNameId.at(kv.second);
            MUST( fwrite( &symbolicId, sizeof(symbolicId), 1, fOut) == 1, "write error");
        } else
        {
            const char* val = kv.second.c_str();
            MUST( fwrite( val, strlen(val)+1, 1, fOut) == 1, "write error");
        }
    }
    
    MUST( ftell(fOut)- numBytes == filePos, "tag set size mismatch");

}

RawTags::RawTagIterator::RawTagIterator(const uint8_t* symbolicNameBits, 
                                        const uint8_t *tagsAtPos, uint64_t pos):
            symbolicNameBits(symbolicNameBits), tagsAtPos(tagsAtPos), pos(pos)
{ }

       
bool RawTags::RawTagIterator::operator!=(const RawTagIterator &other)
{
    return tagsAtPos != other.tagsAtPos;
}
        
RawTags::RawTagIterator& RawTags::RawTagIterator::operator++()
{
    tagsAtPos += (isSymbolicName(pos*2)  ? 1 : strlen( (char*)tagsAtPos) + 1);
    tagsAtPos += (isSymbolicName(pos*2+1)? 1 : strlen( (char*)tagsAtPos) + 1);
    pos += 1;
    
    return *this;
}

std::pair<const char*, const char*> RawTags::RawTagIterator::operator*()
{
    const char* key;
    const char* value;
    
    key = isSymbolicName(pos*2) ? symbolicNames[ *tagsAtPos ] : (char*)tagsAtPos;
    const uint8_t *valuePos = tagsAtPos + 
        (isSymbolicName(pos*2) ? 1 : strlen((char*)tagsAtPos) + 1);
    
    value = isSymbolicName(pos*2+1) ? symbolicNames[ *valuePos ] : (char*)valuePos;
    
    return std::make_pair(key, value);
}

bool RawTags::RawTagIterator::isSymbolicName( int idx) const
{
    int byteIdx = idx / 8;
    int bitIdx  = idx % 8;
    
    return symbolicNameBits[byteIdx] & (1 << (7 - bitIdx));
}

