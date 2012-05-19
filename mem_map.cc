
#include "mem_map.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

mmap_t init_mmap ( const char* file_name)
{
    mmap_t res;
    res.fd = open(file_name, O_CREAT|O_RDWR, S_IRUSR | S_IWUSR);  
    
    struct stat stats;
    if (0 != fstat(res.fd, &stats)) { perror ("fstat"); exit(0); }

    assert( stats.st_size % 4096 == 0);
    if (stats.st_size == 0)
        res.ptr = NULL;
    else
    {
        res.ptr = mmap(NULL, stats.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, res.fd, 0);
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

    if (map->ptr)
    {
        if (0 != munmap(map->ptr, map->size)) { perror("munmap"); exit(0);}
    }
    size_t ps = sysconf (_SC_PAGESIZE);
    
    //increase map file by 50% to reduce the number of times that a resize is necessary;
    size_t new_file_size = ( (size + ps)*3/2)/ps*ps;   
    //size_t new_file_size = (size+ps-1)/ps*ps;
    assert(new_file_size % ps == 0); //needs to be a multiple of page size for mmap
    if (new_file_size < size) { printf("error resizing memory map\n"); exit(0);}
    if (0 != ftruncate(map->fd, new_file_size)) { perror("[ERR]"); exit(0);}
   
    //std::cout << "Resizing file to hold " << file_no_nodes << " nodes (" << (file_no_nodes / ps) << " pages)" << std::endl;
    
    map->ptr = mmap(NULL, new_file_size, PROT_WRITE, MAP_SHARED, map->fd, 0);
    if (map == MAP_FAILED) { perror("[ERR]"); exit(0);}
    map->size = new_file_size;
}

