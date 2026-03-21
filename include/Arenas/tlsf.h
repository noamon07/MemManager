#ifndef TLSF_ALLOCATOR_H
#define TLSF_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/arena.h"

#define TLSF_FL_INDEX_MAX 32
#define TLSF_SL_INDEX_COUNT 16

/* 
 * The node placed inside the payload of a FREE block.
 * Because BaseHeader is 8 bytes, FreeNode is 8 bytes, and BaseFooter is 4 bytes,
 * the absolute MINIMUM block size for the General Arena will be 24 bytes (aligned).
 */
typedef struct {
    BaseHeader header;
    uint32_t prev_free;
    uint32_t next_free;
} FreeBlockHeader;

/* Agnostic state tracker for free holes */
typedef struct {
    uint32_t fl_bitmap;
    uint32_t sl_bitmap[TLSF_FL_INDEX_MAX];
    uint32_t blocks[TLSF_FL_INDEX_MAX][TLSF_SL_INDEX_COUNT];
} TLSFAllocator;

/* Initializes / Wipes the TLSF index */
int tlsf_init(TLSFAllocator* tlsf);

/* 
 * Wipes out the TLSF. 
 * This is meant to be called instantly after bump_defrag() because 
 * defragging slides all live objects together, eliminating all holes! 
 */
void tlsf_clear(TLSFAllocator* tlsf);

/* Registers a new free hole into the appropriate bin */
void tlsf_insert(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset);

/* Removes a specific free hole from its bin (used during coalescing) */
void tlsf_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset);

/* 
 * $O(1)$ Search for a free hole. 
 * Returns INVALID_DATA_OFFSET if no hole is large enough.
 */
uint32_t tlsf_find_and_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t size);

#endif