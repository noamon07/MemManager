#ifndef NURSERY_H
#define NURSERY_H

#include <stdint.h>
#include "strategies/bump.h"



typedef struct Nursery {
    uint8_t* mem;
    uint32_t mem_size;
    uint32_t cur_index;
} Nursery;

/* Initialization and teardown */
void mm_nursery_destroy();
void mm_nursery_reset();
void* mm_malloc_nursery(uint32_t size, uint32_t** entry_index);
void mm_free_nursery(void* ptr);
void* mm_realloc_nursery(void* ptr, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index);
void* mm_calloc_nursery(uint32_t size, uint32_t** entry_index);

#endif