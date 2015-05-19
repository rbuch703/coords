
#ifndef RAW_TAGS_H
#define RAW_TAGS_H

#include <map>
#include <vector>
#include <string>

#include "misc/symbolicNames.h"

typedef std::vector< std::pair<std::string, std::string> > TTags;

class RawTags 
{
public:
    RawTags(uint64_t numTags, uint64_t numTagBytes, const uint8_t *symbolicNameBits, 
            const uint8_t* tagsStart);
            
    RawTags(const uint8_t* src);

    static void     serialize( const TTags &tags, FILE* fOut);
    static uint64_t getSerializedSize( const TTags &tags);

    class RawTagIterator {
    public:
        RawTagIterator(const uint8_t* symbolicNameBits, const uint8_t *tagsAtPos, uint64_t pos);
        bool operator!=(const RawTagIterator &other);
        RawTagIterator& operator++();
        std::pair<const char*, const char*> operator*();        

	private:
        bool isSymbolicName( int idx) const;
        
    private:
        const uint8_t *symbolicNameBits;
        const uint8_t *tagsAtPos;
        uint64_t pos;
    };
    
    RawTagIterator begin() const { return RawTagIterator(symbolicNameBits, tagsStart, 0);}
    RawTagIterator end()   const { return RawTagIterator(symbolicNameBits, tagsStart + numTagBytes, numTags);}
   
private:
    uint64_t numTags;
    uint64_t numTagBytes;
    const uint8_t *symbolicNameBits;
    const uint8_t *tagsStart;
};

#endif
