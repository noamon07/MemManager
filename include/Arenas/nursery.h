#ifndef NURSERY_H
#define NURSERY_H

#include <stdint.h>
#include "Arenas/handle.h"

#define INVALID_NURSERY_INDEX ((uint32_t)-1)

typedef struct Nursery {
    uint8_t* mem;
    uint32_t mem_size;
    uint32_t cur_index;
    uint32_t alloc_memory;
} Nursery;


/* Initialization and teardown */
void mm_nursery_destroy();
uint32_t mm_malloc_nursery(uint32_t size, uint32_t** entry_index);
void mm_free_nursery(data_pos data);
uint32_t mm_realloc_nursery(data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index);
uint32_t mm_calloc_nursery(uint32_t size, uint32_t** entry_index);
void* mm_nursery_get(HandleEntry* entry);
Nursery* mm_get_nursery_instance(void);
#endif