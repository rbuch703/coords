
#ifndef RAW_TAGS_H
#define RAW_TAGS_H

#include <map>
#include <vector>
#include <string>

#ifndef COORDS_MAPNIK_PLUGIN
    #include "misc/symbolicNames.h"
    #include "containers/chunkedFile.h"
#else
    #include "symbolicNames.h"
#endif

typedef std::vector< std::pair<std::string, std::string> > Tags;

class RawTags 
{
public:
    RawTags(uint64_t numTags, uint64_t numTagBytes, const uint8_t *symbolicNameBits, 
            const uint8_t* tagsStart);
            
    RawTags(const uint8_t* src);

    
    static void     serialize( const Tags &tags, FILE* fOut);
#ifndef COORDS_MAPNIK_PLUGIN
    static void     serialize( const Tags &tags, Chunk &chunk);
#endif
    static uint64_t getSerializedSize( const Tags &tags);
    
    uint64_t getSerializedSize() const;
    std::map< std::string, std::string> asDictionary() const;

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
