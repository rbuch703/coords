
#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h> //since MUST() needs printf()

#ifndef MUST
    //a macro that is similar to assert(), but is not deactivated by NDEBUG
    #define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); abort();}}
#endif

static const uint64_t IS_WAY_REFERENCE = 0x8000000000000000ull;


/** 0x7FFFFFFF is the maximum positive value in signed ints, i.e. ~ 2.1 billion
 *  In OSMs int32_t lat/lng storage, this corresponds to ~ 210°, which is outside
 *  the valid range for latitude and longitude, and thus can be used to mark
 *  invalid entries.
 *  note: 0xFFFFFFFF in two's complement is -1, and thus a valid lat/lng value.
 **/ 
const int32_t INVALID_LAT_LNG = 0x7FFFFFFF;


static const uint64_t BUCKET_SIZE                   = 10000000;
static const uint64_t NODES_OF_WAYS_BUCKET_SIZE     = 1000000;
static const uint64_t WAYS_OF_RELATIONS_BUCKET_SIZE = 100000;

#endif
