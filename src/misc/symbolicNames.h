
#ifndef SYMBOLIC_NAMES_H
#define SYMBOLIC_NAMES_H

#include <stdint.h>
#include "containers/radixTree.h"

extern const char* symbolicNames[];
extern const uint64_t numSymbolicNames;
extern const RadixTree<uint8_t> symbolicNameId;

#endif

