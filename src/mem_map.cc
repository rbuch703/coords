
#include "mem_map.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

//#include <iostream>

#ifndef MUST
#define MUST(action, errMsg) { if (!(action)) {printf("Error: '%s' at %s:%d, exiting...\n", errMsg, __FILE__, __LINE__); abort();}}
#endif

mmap_t init_mmap ( const char* file_name, bool readable, bool writeable, bool clearContents)
{
    assert(readable || writeable);  //no use otherwise
    
    mmap_t res;
    /** the underlying file always needs to be readable. Even if the map itself is only writeable,
        the OS needs to be able to *read* the whole data page which is changed by *writing* */
    res.fd = open(file_name, O_CREAT|(writeable? O_RDWR : O_RDONLY), S_IRUSR | S_IWUSR);  
    assert(res.fd != -1);
    
    if (clearContents)
        MUST(0 == ftruncate(res.fd, 0), "cannot truncate file");
        
    struct stat stats;
    if (0 != fstat(res.fd, &stats)) { perror ("fstat"); exit(0); }

    assert( stats.st_size % 4096 == 0);
    if (stats.st_size == 0)
        res.ptr = NULL;
    else
    {
        res.ptr = mmap(NULL, stats.st_size, (writeable? PROT_WRITE:0) | (readable?PROT_READ:0), MAP_SHARED, res.fd, 0);
        if (res.ptr == MAP_FAILED) { perror("mmap"); exit(0);}
    }
    
/*    if (stats.st_size == 0)
    {
        size_t ps = sysconf (_SC_PAGESIZE);
        if (0 !=ftruncate(fd, ps))
            {perror("ftruncate"); exit(0); }
        if (0 != fstat(fd, &stats)) { perror ("fstat"); exit(0); }
    }
  */  
    res.size = stats.st_size;
    return res;
}

void free_mmap ( mmap_t *map)
{
    if (map->ptr)
    {
        if (0 != munmap(map->ptr, map->size)) { perror("munmap"); exit(0);}
    }
    map->ptr = 0;
    assert( map->fd > 2);
    close(map->fd);
    map->fd = -1;
    map->size = 0;
}

void ensure_mmap_size( mmap_t *map, uint64_t size)
{
    if (map->size >= size) return;

    //std::cout << "File can hold at most " << file_no_nodes << " nodes, but " << node_id << " nodes need to be stored" << std::endl;

    /*if (map->ptr)
    {
        if (0 != munmap(map->ptr, map->size)) { perror("munmap"); exit(0);}
    }*/
    size_t ps = sysconf (_SC_PAGESIZE);
    
    //increase map file by 10% to reduce the number of times that a resize is necessary;
    size_t new_file_size = ( ( size * 11 / 10) / ps +1) * ps;
    //size_t new_file_size = (size+ps-1)/ps*ps;
    assert(new_file_size % ps == 0); //needs to be a multiple of page size for mmap
    if (new_file_size < size) { printf("error resizing memory map\n"); exit(0);}
    if (0 != ftruncate(map->fd, new_file_size)) { perror("[ERR]"); exit(0);}
   
    //std::cout << "Resizing map " << map->size << " -> " << new_file_size << std::endl;
    
    if (map->ptr)
        map->ptr = mremap(map->ptr, map->size, new_file_size, MREMAP_MAYMOVE);
    else
        map->ptr = mmap(NULL, new_file_size, PROT_WRITE, MAP_SHARED, map->fd, 0);
        
    if (map->ptr == MAP_FAILED) { perror("[ERR]"); exit(0);}
    map->size = new_file_size;
}

