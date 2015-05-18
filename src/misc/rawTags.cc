

#include "rawTags.h"
#include <string.h> //for strlen

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

