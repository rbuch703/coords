
#ifndef OSM_TAGS_H
#define OSM_TAGS_H

#include <stdint.h>

/** list of keys from key-value pairs that do not add relevant information in the context of a map rendering application,
    
    the purpose of this list is to reduce the number of *different* key-value pairs that need to be handled,
    in order for the whole list of remaining pairs to fit nicely into pc memory*/
extern const char * ignore_keys[];
extern uint32_t num_ignore_keys;
#endif
