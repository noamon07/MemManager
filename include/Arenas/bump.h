#ifndef BUMP_ALLOCATOR_H
#define BUMP_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/arena.h"

/**
 * @brief Defragmentation Callback Protocol
 * 
 * Triggered during the bump_defrag pass for every live object.
 * 
 * @param arena_context Pointer to the parent management structure.
 * @param header The BaseHeader of the block currently being moved.
 * @param new_offset The new physical offset within the bump buffer.
 * @return int 1 to KEEP the block in this arena (Compact).
 *             0 to DROP/PROTOME the block (Remove from Nursery).
 */
typedef int (*bump_defrag_cb)(void* arena_context, BaseHeader* header, uint32_t new_offset);

/**
 * @struct BumpAllocator
 * @brief Manages a contiguous memory region for sequential allocation.
 */
typedef struct {
    uint8_t* mem;              /* Start of the raw memory buffer */
    uint32_t mem_size;         /* Current capacity of the buffer in bytes */
    uint32_t max_allowed_size; /* Hard limit for growth/expansion */
    uint32_t cur_index;        /* The Bump Pointer (offset for next allocation) */
    uint32_t alloc_memory;     /* Total bytes currently used by live objects */
    
    bump_defrag_cb defrag_cb;  /* Callback for handle updates and promotion */
    void* arena_context;       /* Context passed to the defrag_cb */
} BumpAllocator;

/* --- Lifecycle Management --- */

/**
 * @brief Initializes the bump allocator.
 * @return 1 on success, 0 on memory failure.
 */
int bump_init(BumpAllocator* bump, uint32_t initial_size, bump_defrag_cb cb, void* context, uint32_t max_allowed_size);

/**
 * @brief Frees the backing memory of the allocator.
 */
void bump_destroy(BumpAllocator* bump);

/* --- Core Allocation Operations --- */

/**
 * @brief Allocates memory sequentially.
 * @param size Requested payload size in bytes.
 * @param handle The indirection handle assigned to this block.
 * @param custom_flags Metadata bits for GC state or object type.
 * @return uint32_t The offset index to the BaseHeader, or INVALID_DATA_OFFSET on failure.
 */
uint32_t bump_malloc(BumpAllocator* bump, uint32_t size, Handle handle, uint32_t custom_flags);

/**
 * @brief Marks a block as logically free.
 * Note: Memory is not reclaimed until bump_defrag() is called.
 * @return 1 on success, 0 if offset is invalid.
 */
uint8_t bump_free(BumpAllocator* bump, uint32_t offset);

/* --- Maintenance & GC --- */

/**
 * @brief Performs sliding compaction.
 * Shifts all live blocks to the start of the buffer to eliminate gaps.
 * Calls defrag_cb for each moved block to update the Handle Table.
 * @return uint32_t Total bytes reclaimed during compaction.
 */
uint32_t bump_defrag(BumpAllocator* bump);

/**
 * @brief Attempts to expand the allocator's backing buffer.
 * @return 1 on success, 0 if max_allowed_size is exceeded.
 */
int bump_grow(BumpAllocator* bump, uint32_t requested_size);

#endif /* BUMP_ALLOCATOR_H */