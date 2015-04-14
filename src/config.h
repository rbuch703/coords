
#ifndef CONFIG_H
#define CONFIG_H

#ifndef MUST
    //a macro that is similar to assert(), but is not deactivated by NDEBUG
    #define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); abort();}}
#endif

static const uint64_t IS_WAY_REFERENCE = 0x8000000000000000ull;

static const uint64_t BUCKET_SIZE                   = 10000000;
static const uint64_t NODES_OF_WAYS_BUCKET_SIZE     = 1000000;
static const uint64_t WAYS_OF_RELATIONS_BUCKET_SIZE = 100000;

#endif
