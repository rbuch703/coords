
#ifndef MEM_MAP_H
#define MEM_MAP_H

#include <stdint.h>

typedef struct mmap_t {
    void* ptr;
    int32_t fd;
    uint64_t size;
} mmap_t;

mmap_t init_mmap ( const char* file_name, bool readable=true, bool writeable=true);
void   free_mmap ( mmap_t *mmap);
void   ensure_mmap_size( mmap_t *mmap, uint64_t size);
//void   sync_mmap(mmap_t *mmap, uint64_t offset, 

#endif
