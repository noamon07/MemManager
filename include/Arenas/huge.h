#ifndef HUGE_H
#define HUGE_H
#include "Arenas/handle.h"

Handle mm_malloc_huge(size_t size);
void mm_free_huge(Handle handle);
Handle mm_realloc_huge(Handle handle, size_t new_size);
Handle mm_calloc_huge(size_t size);
#endif