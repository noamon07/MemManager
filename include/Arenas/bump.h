#ifndef BUMP_H
#define BUMP_H

#include <stdint.h>
#include "Arenas/handle.h"

#define INVALID_BUMP_INDEX ((uint32_t)-1)

/* * Every arena's custom header MUST start with these exact fields.
 * This allows bump.c to merge blocks without knowing your custom data.
 * You can use the 'custom_flags' for your generations!
 */
typedef struct {
    uint32_t size:28,
             is_allocated:1,
             before_alloc:1,
             custom_flags:2; 
    uint32_t entry_index;
} BaseHeader;

typedef struct BumpAllocator BumpAllocator;

/* The caller provides their own logic for defragging/promoting */
typedef void (*DefragFunction)(BumpAllocator* allocator);

struct BumpAllocator {
    uint8_t* mem;
    uint32_t mem_size;
    uint32_t cur_index;
    uint32_t alloc_memory;
    
    /* Configured by whoever uses the allocator */
    uint32_t header_size;
    DefragFunction defrag_logic;
};

/* Initialization and teardown */
int mm_bump_init(BumpAllocator* allocator, uint32_t start_size, uint32_t header_size, DefragFunction defrag_logic);
void mm_bump_destroy(BumpAllocator* allocator);

/* Core Allocation API */
uint32_t mm_malloc_bump(BumpAllocator* allocator, uint32_t size, uint32_t** entry_index);
uint32_t mm_calloc_bump(BumpAllocator* allocator, uint32_t size, uint32_t** entry_index);
uint32_t mm_realloc_bump(BumpAllocator* allocator, data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index);
void mm_free_bump(BumpAllocator* allocator, data_pos data);

/* Utility */
void* mm_bump_get(BumpAllocator* allocator, HandleEntry* entry);

/* Exposed for the caller's defrag function to use */
void mm_bump_merge_before(BumpAllocator* allocator, BaseHeader** _header);
void mm_bump_trim_block(BumpAllocator* allocator, BaseHeader* header, uint32_t new_size);

#endif