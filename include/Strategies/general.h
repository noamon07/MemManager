#ifndef GENERAL_H
#define GENERAL_H

#include "Arenas/arena.h"
#include "Arenas/handle.h"
#include "Arenas/tlsf.h"

/* * The General Arena is a classic Freelist allocator backed by TLSF.
 * Unlike the Nursery, it does NOT use a bump allocator and NEVER defragments.
 * Blocks stay exactly where they are placed until they are freed.
 */
typedef struct {
    uint8_t* mem;           // The physical RAM for the arena
    uint32_t mem_size;      // Total capacity
    uint32_t alloc_memory;  // Active payload tracker
    
    TLSFAllocator tlsf;     // The O(1) indexer for free holes
    char last_block_allocated;
} General;

/* Initialize the arena, allocate its physical memory, and format the first massive free block */
int general_init(uint32_t initial_size);

/* Destroy the arena and free the physical RAM */
void general_destroy(void);

/* * 1. Calculate requested block size (payload + headers + alignment)
 * 2. Ask TLSF for a free hole (tlsf_find_and_remove)
 * 3. Split the hole if it's too big (trim_block) and insert the scrap back into TLSF
 * 4. Format the header and update the Handle Table
 */
uint32_t general_malloc(uint32_t size, Handle handle);

/* * 1. Mark block as free.
 * 2. Check if the NEXT physical block is free. If so, `tlsf_remove` it and merge!
 * 3. Check if the PREV physical block is free. If so, `tlsf_remove` it and merge!
 * 4. `tlsf_insert` the newly combined massive hole.
 */
void general_free(uint32_t offset);

/* * Expand or shrink a block in place if possible (using neighbor logic).
 * If not possible, ll_malloc a new hole, memmove, and ll_free the old hole.
 */
uint32_t general_realloc(uint32_t new_size, Handle handle);

/* Expose the pointer for the Handle Table */
void* general_get(HandleEntry* entry);
General* get_general_instance(void);

#endif