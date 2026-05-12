#ifndef SLAB_H
#define SLAB_H

#include "arena.h"
#include <stddef.h>
#include <stdint.h>

/** 
 * Minimum size to ensure a free slot can hold the 'next' pointer (uint32_t offset). 
 */
#define MIN_SLAB_OBJ_SIZE sizeof(uint32_t)

/**
 * @struct Slab
 * @brief Control structure for a fixed-size object pool.
 */
typedef struct {
    uint32_t object_size;    /* Size of each individual slot in bytes */
    uint32_t free_list_head; /* Offset to the first available free slot */
    
    uint32_t slab_size;        /* Total current size of the managed memory region */
    uint32_t max_allowed_size; /* Limit for expansion */
} Slab;

/**
 * @brief Initializes a Slab allocator instance.
 * 
 * Sets the object size (ensuring it meets MIN_SLAB_OBJ_SIZE) and 
 * resets the free list. Note: Does not allocate the backing memory.
 * 
 * @param slab Pointer to the Slab structure.
 * @param obj_size Size of objects this slab will manage.
 */
void slab_init(Slab* slab, uint32_t obj_size);

/**
 * @brief Allocates a fixed-size slot from the slab.
 * 
 * Uses the free_list_head to return the next available offset in O(1).
 * 
 * @param slab Pointer to the Slab structure.
 * @param memory_base Pointer to the start of the raw memory buffer.
 * @return uint32_t Offset to the allocated slot, or INVALID_DATA_OFFSET if full.
 */
uint32_t slab_alloc(Slab* slab, void* memory_base);

/**
 * @brief Returns a slot to the slab.
 * 
 * Inserts the slot back into the head of the free list in O(1).
 * 
 * @param slab Pointer to the Slab structure.
 * @param offset The offset of the slot being freed.
 * @param memory_base Pointer to the start of the raw memory buffer.
 */
void slab_free(Slab* slab, uint32_t offset, void* memory_base);

/**
 * @brief Resizes or re-initializes the managed memory region.
 * 
 * Useful when the backing buffer is expanded or moved. Re-builds the 
 * internal free list pointers based on the new region size.
 * 
 * @param slab Pointer to the Slab structure.
 * @param region_size New size of the memory buffer.
 * @param memory_base Pointer to the start of the memory buffer.
 */
void slab_change_region(Slab* slab, uint32_t region_size, void* memory_base);

#endif // SLAB_H