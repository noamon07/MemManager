#ifndef BUMP_ALLOCATOR_H
#define BUMP_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/arena.h"

 
// Callback triggered during compaction.
// Return 1 to MOVE and KEEP the block in this arena.
// Return 0 to DROP the block (e.g., it was promoted elsewhere or destroyed).
typedef int (*bump_defrag_cb)(void* arena_context, BaseHeader* header, uint32_t new_offset);

typedef struct {
    uint8_t* mem;
    uint32_t mem_size;
    uint32_t max_allowed_size;
    uint32_t cur_index;
    uint32_t alloc_memory;
    
    bump_defrag_cb defrag_cb;
    void* arena_context; /* Pointer back to the parent Nursery or Long-Lived struct */
} BumpAllocator;


int bump_init(BumpAllocator* bump, uint32_t initial_size, bump_defrag_cb cb, void* context, uint32_t max_allowed_size);
void bump_destroy(BumpAllocator* bump);

// Core Allocation (Returns the offset index of the BaseHeader)
uint32_t bump_malloc(BumpAllocator* bump, uint32_t size, Handle handle, uint32_t custom_flags);

// Marks block as free
uint8_t bump_free(BumpAllocator* bump, uint32_t offset);

// Triggers a sliding compaction. Returns the number of bytes reclaimed.
uint32_t bump_defrag(BumpAllocator* bump);

// Fallback growth logic
int bump_grow(BumpAllocator* bump, uint32_t requested_size);

#endif