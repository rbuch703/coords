
#ifndef MEM_MAP_H
#define MEM_MAP_H

#include <stdint.h>

typedef struct mmap_t {
    void* ptr;
    int32_t fd;
    uint64_t size;
} mmap_t;

//temporary workaround while we are mixing C and C++ code
#ifdef __cplusplus
 extern "C" {
 #endif 

mmap_t init_mmap ( const char* file_name);
void   free_mmap ( mmap_t *mmap);
void   ensure_mmap_size( mmap_t *mmap, uint64_t size);

#ifdef __cplusplus
}
#endif 

#endif
