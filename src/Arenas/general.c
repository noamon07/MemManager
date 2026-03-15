#include "Arenas/general.h"
#include <stddef.h>

// Stub implementation to allow linking
allocator_t* allocator_init(void* memory_pool, size_t pool_size) {
    (void)memory_pool;
    (void)pool_size;
    return NULL;
}

void* mm_general_alloc(size_t size, alloc_type_t* type) {
    (void)size;
    (void)type;
    return NULL;
}

void allocator_free(allocator_t* alloc, void* ptr) {
    (void)alloc;
    (void)ptr;
}

void* allocator_realloc(allocator_t* alloc, void* ptr, size_t new_size) {
    (void)alloc;
    (void)ptr;
    (void)new_size;
    return NULL;
}

void allocator_destroy(allocator_t* alloc) {
    (void)alloc;
}

void allocator_dump_stats(allocator_t* alloc) {
    (void)alloc;
}
