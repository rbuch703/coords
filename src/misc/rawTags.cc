

#include "rawTags.h"
#include "varInt.h"
#include "config.h"
#include <string.h> //for strlen
#include "containers/chunkedFile.h"

/* ON-DISK LAYOUT FOR TAGS:
    varUInt numBytes, *NOT* including this field itself
    varUInt numTags (one tag = two names (key + value)
    uint8_t  isSymbolicName[ceil( (numTags*2)/8)] (bit array)
    
    for each name:
    1. if is symbolic --> one uint8_t index into symbolicNames
    2. if is not symbolic --> zero-terminated string
*/

RawTags::RawTags(const uint8_t* src, uint64_t *nBytesRead)
{
    int nRead = 0;
    
    uint32_t totalNumBytes = varUintFromBytes(src, &nRead);

    if (nBytesRead)
        *nBytesRead = totalNumBytes + nRead;
    
    //std::cout << "\thas " << numTagBytes << "b of tags."<< std::endl;
    src += nRead;
    
    this->numTags = varUintFromBytes(src, &nRead);
    src += nRead;
    
    //uint8_t *symbolicNameBytes = pos;
    uint64_t numNames = numTags * 2; // key and value per tag
    uint64_t numSymbolicNameBytes = (numNames + 7) / 8;
    
    this->numTagBytes = totalNumBytes - numSymbolicNameBytes - varUintNumBytes(numTags);
    this->symbolicNameBits = src;
    this->tagsStart = src + numSymbolicNameBytes;
    
}



uint64_t RawTags::getSerializedSize(const Tags &tags)
{
    uint64_t numTags = tags.size();
    uint64_t numNames = numTags * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames +7) / 8; //one bit per name --> have to round up
    
    uint64_t numTagBytes = 0;
    for (const std::pair<std::string, std::string> &kv: tags)
    {
        numTagBytes += symbolicNameId.at(kv.first.c_str()) ? 1 : kv.first.length()  + 1;
        numTagBytes += symbolicNameId.at(kv.second.c_str()) ? 1 : kv.second.length() + 1;
    }
    
    uint64_t numBytes = varUintNumBytes(numTags) +
                        bitfieldSize * sizeof(uint8_t) +
                        numTagBytes;
                        
    return numBytes;

}

uint64_t RawTags::getSerializedSize() const
{
    uint64_t numNames = numTags * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames + 7) / 8; //one bit per name --> have to round up

    uint64_t numBytes = varUintNumBytes(this->numTags) + 
                        bitfieldSize * sizeof(uint8_t) +
                        numTagBytes;
    
    return numBytes;
}

uint64_t RawTags::serialize( uint8_t * const outputBuffer) const
{
    uint8_t *outPos = outputBuffer;
    
    uint64_t numBytes = this->getSerializedSize();
    outPos += varUintToBytes(numBytes, outPos);
    outPos += varUintToBytes(this->numTags, outPos);
    
    uint64_t numNames = numTags * 2;
    uint64_t bitfieldSize = ( numNames + 7) / 8; //one bit per name --> have to round up
    memcpy(outPos, symbolicNameBits, bitfieldSize);
    outPos += bitfieldSize;
    
    memcpy(outPos, tagsStart, numTagBytes);
    outPos += numTagBytes;
    
    return outPos - outputBuffer;
}   


std::map< std::string, std::string> RawTags::asDictionary() const
{
    std::map< std::string, std::string> res;
    
    for (const std::pair<const char*, const char*> &kv : *this)
        res.insert( std::make_pair(kv.first, kv.second));
        
    return res;
}

void RawTags::serialize(const Tags &tags, FILE* fOut)
{
    uint64_t numBytes = 0;
    uint8_t *bytes = serialize(tags, &numBytes);
    
    MUST( fwrite(bytes, numBytes, 1, fOut) == 1, "write error");
    delete [] bytes;
}

#ifndef COORDS_MAPNIK_PLUGIN
void RawTags::serialize(const Tags &tags, Chunk& chunk)
{
    uint64_t numBytes = 0;
    uint8_t *bytes = serialize(tags, &numBytes);
    
    chunk.put(bytes, numBytes);
    delete [] bytes;
}
#endif


uint8_t* RawTags::serialize( const Tags &tags, uint64_t *numBytesOut)
{
    uint64_t numBytes = getSerializedSize(tags);
    uint64_t numBytesIncludingSizeField = numBytes + varUintNumBytes(numBytes);
    if (numBytesOut)
        *numBytesOut = numBytesIncludingSizeField;
        
    uint8_t *outBuf = new uint8_t[numBytesIncludingSizeField];
    
    RawTags::serialize(tags, numBytes, outBuf, numBytesIncludingSizeField);

    //MUST( outPos - outStart == (int64_t)numBytes, "tag set size mismatch");
    return outBuf;
}

void RawTags::serialize( const Tags &tags, uint64_t numBytes, 
                             uint8_t *outputBuffer, uint64_t outputBufferSize)
{
    uint8_t *outPos = outputBuffer;
    
    outPos += varUintToBytes(numBytes, outPos);
    uint8_t *outStart = outPos; //byte at which the memory area of size 'numBytes' starts

    outPos += varUintToBytes(tags.size(), outPos);

    if ( tags.size() == 0)
    {
        MUST( outPos - outStart == (int64_t)numBytes, "tag set size mismatch");
        MUST( outPos - outputBuffer == (int64_t)outputBufferSize, "tag set size mismatch");

        return;
    }
        
    uint64_t numNames = tags.size() * 2;    //one key, one value
    uint64_t bitfieldSize = (numNames + 7) / 8; //one bit per name --> have to round up
    uint8_t *isSymbolicName = outPos;
    memset(isSymbolicName, 0, bitfieldSize);

    outPos += bitfieldSize;
    
    int idx = 0;
    for ( const std::pair<std::string, std::string> &kv : tags)
    {
        int byteIdx = idx / 8;
        int bitIdx  = 7 - (idx % 8);
        const uint8_t *symbolicKey = symbolicNameId.at(kv.first.c_str());
        if (symbolicKey)
        {
            isSymbolicName[byteIdx] |= (1 << bitIdx);
            *(outPos++) =  *symbolicKey;
        } else
        {
            const char* key = kv.first.c_str();
            int terminatedLength = strlen(key) + 1;
            memcpy(outPos, key, terminatedLength);
            outPos += terminatedLength;
        }
        
        MUST( bitIdx > 0, "logic error");
        bitIdx -= 1;

        const uint8_t *symbolicValue = symbolicNameId.at(kv.second.c_str());
        if (symbolicValue)
        {
            isSymbolicName[byteIdx] |= (1 << bitIdx);
            *(outPos++) = *symbolicValue;
        } else
        {
            const char* val = kv.second.c_str();
            int terminatedLength = strlen(val) + 1;
            memcpy(outPos, val, terminatedLength);
            outPos += terminatedLength;
        }
        
        idx += 2;
    }

    MUST( outPos - outputBuffer   == (int64_t)outputBufferSize, "tag set size mismatch");

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

