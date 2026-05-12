#ifndef NURSERY_H
#define NURSERY_H

#include "Arenas/arena.h"
#include "Arenas/bump.h"

/** 
 * Threshold for survivor promotion. 
 * Objects surviving 3 compaction cycles move to the General Arena.
 */
#define NURSERY_PROMOTION_GENERATION 3

/**
 * @struct Nursery
 * @brief Wrapper for the Bump Allocator managing the young generation.
 */
typedef struct {
    BumpAllocator bump; /* The underlying sequential allocation logic */
} Nursery;

/* --- Lifecycle --- */

/**
 * @brief Initializes the Nursery arena.
 * 
 * Sets up the internal Bump Allocator and registers the promotion 
 * callback logic.
 * 
 * @param initial_size Starting memory capacity for the Nursery.
 * @param max_allowed_size Maximum expansion limit for the Nursery region.
 * @return Nursery* Pointer to the initialized Nursery instance.
 */
Nursery* nursery_init(uint32_t initial_size, uint32_t max_allowed_size);

/**
 * @brief Shuts down the Nursery and releases backing memory.
 */
void nursery_destroy();

/* --- Core Allocation --- */

/**
 * @brief Allocates a block in the Nursery.
 * 
 * Extremely fast O(1) operation that advances the bump pointer.
 * 
 * @param size Requested payload size.
 * @param handle The indirection handle to be associated with this block.
 * @return uint32_t Offset within the Nursery buffer.
 */
uint32_t nursery_malloc(uint32_t size, Handle handle);

/**
 * @brief Attempts to resize a block within the Nursery.
 */
uint32_t nursery_realloc(uint32_t new_size, Handle handle);

/**
 * @brief Marks a Nursery block as free.
 * Note: Memory is only truly reclaimed during the next compaction phase.
 */
void nursery_free(uint32_t offset);

/* --- Utility --- */

/**
 * @brief Resolves a Nursery offset to a raw physical pointer.
 */
void* nursery_get(uint32_t offset);

/**
 * @brief Singleton accessor for the system's Nursery instance.
 */
Nursery* get_nursery_instance(void);

#endif /* NURSERY_H */