#ifndef MM_API_H
#define MM_API_H
#include <stdint.h>

typedef struct {
    uint32_t index:24,
             generation:8;
} Handle;

#define INVALID_INDEX ((1U << 24) - 1)

int mm_init();
void mm_destroy();
void* mm_get_ptr(Handle handle);
Handle mm_malloc(uint32_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, uint32_t new_size);
Handle mm_calloc(uint32_t size);


#endif