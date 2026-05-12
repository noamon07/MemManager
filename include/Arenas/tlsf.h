#ifndef TLSF_ALLOCATOR_H
#define TLSF_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/arena.h"

/* Configuration for 32-bit address space */
#define TLSF_FL_INDEX_MAX 32   /* 2^32 max bytes */
#define TLSF_SL_INDEX_COUNT 16 /* 16 subdivisions per FL class */

/**
 * @struct FreeBlockHeader
 * @brief Overlays a BaseHeader when a block is free to form a doubly-linked list.
 */
typedef struct {
    BaseHeader header;   /* Standard block metadata */
    uint32_t prev_free;  /* Offset to the previous free block in the same bin */
    uint32_t next_free;  /* Offset to the next free block in the same bin */
} FreeBlockHeader;

/**
 * @struct TLSFAllocator
 * @brief The management structure containing the bitmask index and bin heads.
 */
typedef struct {
    uint32_t fl_bitmap;                            /* Bitmask of non-empty FL classes */
    uint32_t sl_bitmap[TLSF_FL_INDEX_MAX];         /* Bitmasks of non-empty SL subdivisions */
    uint32_t blocks[TLSF_FL_INDEX_MAX][TLSF_SL_INDEX_COUNT]; /* Heads of free list bins */
} TLSFAllocator;

/* --- Lifecycle --- */

/**
 * @brief Initializes the TLSF structure.
 * Clears all bitmaps and sets all bin heads to INVALID_DATA_OFFSET.
 */
int tlsf_init(TLSFAllocator* tlsf);

/**
 * @brief Resets the TLSF allocator state.
 */
void tlsf_clear(TLSFAllocator* tlsf);

/* --- Bin Management --- */

/**
 * @brief Inserts a free memory block into the appropriate TLSF bin.
 * Calculates the FL and SL indices based on the block size and updates bitmaps.
 * 
 * @param tlsf Pointer to the TLSF allocator.
 * @param mem_base The base address of the arena memory.
 * @param offset Offset to the block to be inserted.
 */
void tlsf_insert(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset);

/**
 * @brief Removes a specific block from the TLSF bins.
 * Typically called when a block is being allocated or merged (coalesced).
 */
void tlsf_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset);

/* --- Allocation Logic --- */

/**
 * @brief Searches for a free block that fits the requested size.
 * 
 * This operation is O(1) and uses bitmask searching to find the optimal 
 * free list bin in constant time.
 * 
 * @param tlsf Pointer to the TLSF allocator.
 * @param mem_base The base address of the arena memory.
 * @param size Requested payload size in bytes.
 * @return uint32_t Offset to the found block, or INVALID_DATA_OFFSET if no fit.
 */
uint32_t tlsf_find_and_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t size);

#endif /* TLSF_ALLOCATOR_H */