
#ifndef VARINT_H
#define VARINT_H

#include <stdio.h>

uint64_t varUintFromBytes(const uint8_t *bytes, int* numBytesRead);
uint64_t varUintFromFile (FILE *f, int* numBytesRead);
int      varUintToBytes(uint64_t valIn, uint8_t out[10]);
int      varUintToFile (uint64_t valIn, FILE *f);
int      varUintNumBytes(uint64_t val);

int64_t  varIntFromBytes(const uint8_t *bytes, int* numBytesRead);
int      varIntToBytes(int64_t valIn, uint8_t out[10]);
int      varIntToFile (int64_t valIn, FILE *f);
int      varIntNumBytes(int64_t val);

#endif
