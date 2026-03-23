#ifndef TLSF_ALLOCATOR_H
#define TLSF_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/arena.h"

#define TLSF_FL_INDEX_MAX 32
#define TLSF_SL_INDEX_COUNT 16


typedef struct {
    BaseHeader header;
    uint32_t prev_free;
    uint32_t next_free;
} FreeBlockHeader;

typedef struct {
    uint32_t fl_bitmap;
    uint32_t sl_bitmap[TLSF_FL_INDEX_MAX];
    uint32_t blocks[TLSF_FL_INDEX_MAX][TLSF_SL_INDEX_COUNT];
} TLSFAllocator;

int tlsf_init(TLSFAllocator* tlsf);

void tlsf_clear(TLSFAllocator* tlsf);

// Registers a new free hole into the appropriate bin
void tlsf_insert(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset);

// Removes a specific free hole from its bin
void tlsf_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset);
 
// Search for a free hole. 
// Returns INVALID_DATA_OFFSET if no hole is large enough. 
uint32_t tlsf_find_and_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t size);

#endif