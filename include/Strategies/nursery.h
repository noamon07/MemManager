#ifndef NURSERY_H
#define NURSERY_H

#include <stdint.h>
#include "Arenas/arena.h"
#include "Arenas/bump.h"
#include "Arenas/handle.h"

/* * The mathematical threshold for promotion. 
 * An object that survives 3 defragmentations is moved to the Long-Lived arena.
 */
#define NURSERY_PROMOTION_GENERATION 3

/* * The Nursery Arena 
 * Wraps the generic bump allocator and adds generational logic.
 */
typedef struct {
    BumpAllocator bump;
} Nursery;

/* Frees the underlying bump allocator memory */
void nursery_destroy();

/* --- Core API --- */
/* * Allocates space in the nursery. 
 * If full, it automatically triggers sliding compaction (and potentially growth).
 * Returns the memory offset, or INVALID_DATA_OFFSET on total failure.
 */
uint32_t nursery_malloc(uint32_t size, Handle handle);

/* * Resizes an existing block.
 * Handles the "frontier expand" optimization internally.
 */
uint32_t nursery_realloc(uint32_t new_size, Handle handle);

/* * Marks a block as dead. 
 * Handles O(1) reverse/forward coalescing and bumps the frontier back if it's the last block.
 */
void nursery_free(uint32_t offset);
void* nursery_get(HandleEntry* entry);
Nursery* get_nursery_instance(void);

#endif
