#ifndef MM_API_H
#define MM_API_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t index:24,
             generation:8;
} Handle;

#define INVALID_INDEX ((1U << 24) - 1)
#define INVALID_HANDLE ((Handle){INVALID_INDEX, 0})

int mm_init(size_t max_size);
void mm_destroy();
void* mm_get_ptr(Handle handle);
Handle mm_malloc(size_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, size_t new_size);
Handle mm_calloc(size_t size);
void mem_set_ref(Handle parent, Handle child);
void mem_remove_ref(Handle parent, Handle child);

#endif