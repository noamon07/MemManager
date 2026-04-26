#ifndef GENERAL_H
#define GENERAL_H

#include "Arenas/arena.h"
#include "Arenas/tlsf.h"

typedef struct {
    uint8_t* mem;           // The physical RAM for the arena
    uint32_t mem_size;      // Total capacity
    uint32_t alloc_memory;  // Active allocated memory tracker
    uint32_t max_allowed_size;
    
    TLSFAllocator tlsf;
    char last_block_allocated;
} General;
// Initialize the arena, allocate its physical memory, and format the first massive free block
General* general_init(uint32_t initial_size);

// Destroy the arena and free the physical RAM
void general_destroy();

uint32_t general_malloc(uint32_t size, Handle handle);

void general_free(uint32_t offset);

uint32_t general_realloc(uint32_t new_size, Handle handle);

void* general_get(uint32_t offset);
General* get_general_instance(void);

#endif