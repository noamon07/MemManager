#ifndef GENERAL_ALLOCATOR_H
#define GENERAL_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/handle.h"


typedef struct General General;

#define INVALID_GENERAL_INDEX ((uint32_t)-1)
void mm_general_destroy();
uint32_t mm_malloc_general(uint32_t size, uint32_t** entry_index);
void mm_free_general(data_pos data);
uint32_t mm_realloc_general(data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index);
uint32_t mm_calloc_general(uint32_t size, uint32_t** entry_index);
void* mm_general_get(HandleEntry* entry);
General* mm_get_general_instance(void);
#endif // GENERAL_ALLOCATOR_H