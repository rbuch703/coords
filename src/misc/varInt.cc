
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <assert.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
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

uint64_t varUintFromBytes(uint8_t *bytes, int* numBytesRead)
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

int varIntToBytes(int64_t valIn, uint8_t out[10])
{
    /* edge case: normally, we extract the sign bit and further process only the absolute value.
     *            But for the smallest int64 (and only that) its absolute value does not fit
     *            into an int64 (negating it yields itself), and would break the algorithm.
     *            So we manually assign a precomputed result in that case 
     */
    if (valIn == 0x8000000000000000ll)
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

int64_t varIntFromBytes(uint8_t *bytes, int* numBytesRead)
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
    if (val == 0x8000000000000000ll) return 10; // edge case, absolute value does not fit int64
    
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


int main()
{

    //for (uint64_t i = 0; i < 0xFFFFFFFF; i++)
    uint64_t i;
    while (1)
    {
        i++;
        if (i % 1000000000 == 0)
            printf("%dM\n", (uint32_t)(i/1000000));
        
        //uint64_t num =   0xFFFFFFFFFFFFFFFFull;
        //uint64_t num = 0x7FFFFFFFFFFFFFFFull;
        int64_t num = ((uint64_t)rand()); 
        
        while (rand() % 2)
            num = (num << 10) ^ rand();

        if (rand() % 2)
            num = -num;

                  
        uint8_t bytes[10];
        varIntToBytes(num, bytes);
        int nBytes;
        int64_t res = varIntFromBytes(bytes, &nBytes);

        //printf("# %" PRIu64 " -> %" PRIu64 "\n", num, res);
        
        //printf("%"PRIi64": %d vs. %d\n", num, nBytes, varIntNumBytes(num));
        assert( num == res && nBytes == varIntNumBytes(num));
        /*if (i != res)
        {
            printf("error at %u (%u)\n", (uint32_t)i, (uint32_t)res);
            assert(0);
        }*/
        
    }   

    /*
    uint8_t tmp[10];
    int numBytes = toBytes(2147483648, tmp);

    for (int i = 0; i < numBytes; i++)
    {
        printf("%X\n", tmp[i]);
        
    }*/
    
//    printf("# %" PRIu64 "\n", fromBytes(tmp, NULL));
    
    return EXIT_SUCCESS;
}
