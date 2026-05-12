#ifndef GENERAL_H
#define GENERAL_H

#include "Arenas/arena.h"
#include "Arenas/tlsf.h"

/**
 * @struct General
 * @brief The state container for the TLSF-managed memory region.
 */
typedef struct {
    uint8_t* mem;              /* The physical RAM buffer for this arena */
    uint32_t mem_size;         /* Current total capacity in bytes */
    uint32_t alloc_memory;     /* Tracker for total currently allocated bytes */
    uint32_t max_allowed_size; /* Ceiling for virtual memory expansion */
    
    TLSFAllocator tlsf;        /* The two-level bitmask and bin indexing logic */
    char last_block_allocated; /* internal flag for block boundary checks */
} General;

/* --- Lifecycle --- */

/**
 * @brief Initializes the General Arena.
 * 
 * Requests the initial memory region from the OS, formats the 
 * primary 'BaseHeader' and 'BaseFooter', and registers the 
 * initial massive free block into the TLSF structure.
 * 
 * @param initial_size Starting size of the arena.
 * @param max_allowed_size Maximum expansion limit.
 * @return General* Pointer to the initialized instance.
 */
General* general_init(uint32_t initial_size, uint32_t max_allowed_size);

/**
 * @brief Destroys the arena and releases the physical RAM back to the OS.
 */
void general_destroy();

/* --- Core Allocation --- */

/**
 * @brief Allocates a block from the General Arena.
 * 
 * Uses TLSF to find the best-fit free block in constant time.
 * 
 * @param size Requested payload size.
 * @param handle The handle associated with this memory.
 * @return uint32_t Offset to the allocated block within 'mem'.
 */
uint32_t general_malloc(uint32_t size, Handle handle);

/**
 * @brief Returns a block to the General Arena.
 * 
 * Triggers immediate coalescing with adjacent free blocks to 
 * prevent fragmentation.
 * 
 * @param offset The physical offset of the block to free.
 */
void general_free(uint32_t offset);

/**
 * @brief Resizes an existing block within the arena.
 * 
 * Attempts to expand in-place if the next block is free, otherwise 
 * performs a move-and-copy operation.
 */
uint32_t general_realloc(uint32_t new_size, Handle handle);

/* --- Utility --- */

/**
 * @brief Resolves a physical offset to a usable void pointer.
 */
void* general_get(uint32_t offset);

/**
 * @brief Singleton accessor for the General Arena instance.
 */
General* get_general_instance(void);

#endif /* GENERAL_H */