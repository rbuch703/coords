
#include <stdio.h>
#include <stdlib.h>
#include "config.h"

int varUintToBytes(uint64_t valIn, uint8_t out[10])
{

    int pos = 0;
    do
    {
        out[pos] = (valIn & 0x7F); //last seven bits
        valIn >>= 7;
        if (valIn)
            out[pos] |= 0x80; // more-bytes-coming marker
        
        pos++;
    } 
    while (valIn);
    
    return pos;
}

int varUintToFile (uint64_t valIn, FILE *f)
{
    uint8_t bytes[10];
    int nBytes = varUintToBytes(valIn, bytes);
    MUST( fwrite( bytes, nBytes, 1, f) == 1, "write error");
    return nBytes;
}


int varUintNumBytes(uint64_t val)
{
    int nBytes = 0;
    do
    {
        val>>= 7;
        nBytes+= 1;
    } while (val);
    
    return nBytes;
}

uint64_t varUintFromBytes(const uint8_t *bytes, int* numBytesRead)
{
    int pos = 0;
    uint64_t res = 0;
    
    int isNotLast;
    do {
        isNotLast = bytes[pos] & 0x80;
        res |= ((uint64_t)(bytes[pos] & 0x7F)) << (7 * pos);
        
        pos += 1;
    
    } while (isNotLast);
    
    if (numBytesRead)
        *numBytesRead = pos;
        
    return res;
}

uint64_t varUintFromFile (FILE *f, int* numBytesRead)
{
    int pos = 0;
    uint64_t res = 0;
    
    int isNotLast;
    do {
        uint8_t byte;
        MUST( fread(&byte, sizeof(byte), 1, f) == 1, "read error");
        isNotLast = byte & 0x80;
        res |= ((uint64_t)(byte & 0x7F)) << (7 * pos);
        pos += 1;
    
    } 
    while (isNotLast);
    
    if (numBytesRead)
        *numBytesRead = pos;
        
    return res;
}

// ======================================================

int varIntToBytes(int64_t valIn, uint8_t out[10])
{
    /* edge case: normally, we extract the sign bit and further process only the absolute value.
     *            But for the smallest int64 (and only that) its absolute value does not fit
     *            into an int64 (negating it yields itself), and would break the algorithm.
     *            So we manually assign a precomputed result in that case 
     */
    if (valIn == (int64_t)0x8000000000000000ll)
    {
        // 6 + 7*6 = 48 = 6 bytes = 12 nibbles
        // residue: 0x8000ll
        static const uint8_t minRes[10] = {0xC0,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x02};
        for (int i = 0; i < 10; i++)
            out[i] = minRes[i];
        return 10;
    }

    out[0] = 0;

    if (valIn < 0)
    {
        out[0] |= 0x40;
        valIn = -valIn;
    }

    
    out[0] |= (valIn & 0x3F);   //last six bits (7th bit is sign bit)
    valIn >>= 6;
    if (valIn)
        out[0] |= 0x80;
    int pos = 1;
    
    while (valIn)
    {
        out[pos] = (valIn & 0x7F); //last seven bits
        valIn >>= 7;
        if (valIn)
            out[pos] |= 0x80; // more-bytes-coming marker
        
        pos+=1;
    }
    
    return pos;
}

int varIntToFile (int64_t valIn, FILE *f)
{
    uint8_t bytes[10];
    int nBytes = varIntToBytes(valIn, bytes);
    MUST( fwrite( bytes, nBytes, 1, f) == 1, "write error");
    return nBytes;
}


int64_t varIntFromBytes(const uint8_t *bytes, int* numBytesRead)
{
    int isNotLast =  bytes[0] & 0x80;
    int isNegative = bytes[0] & 0x40;
    int64_t res =   bytes[0] & 0x3F;
    int pos = 1;
    
    while (isNotLast)
    {
        isNotLast = bytes[pos] & 0x80;
        res |= ((uint64_t)(bytes[pos] & 0x7F)) << ((7 * pos) - 1);  //first byte had only 6 bits
        pos += 1;
    }
    
    if (isNegative)
        res = -res;
    
    if (numBytesRead)
        *numBytesRead = pos;
        
    return res;
}

int varIntNumBytes(int64_t val)
{
    if (val == (int64_t)0x8000000000000000ll) return 10; // edge case, absolute value does not fit int64
    
    if (val < 0)
        val = -val;
    
    val >>= 6;  //first bytes stores only six bits of data (plus sign bit)
    int nBytes = 1;

    while (val)
    {
        val>>= 7;
        nBytes+= 1;
    }
    
    return nBytes;
}
