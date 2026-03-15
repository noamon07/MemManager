#ifndef HUGE_H
#define HUGE_H
#include "Arenas/handle.h"

void* mm_malloc_huge(uint32_t size);
void mm_free_huge(data_pos data, uint32_t size);
void* mm_realloc_huge(data_pos data, uint32_t ptr_size, uint32_t new_size);
void* mm_calloc_huge(uint32_t size);
void* mm_huge_get(HandleEntry* entry);

#endif