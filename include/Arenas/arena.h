#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>
#include "Arenas/handle.h" // Assuming this contains your Handle struct definition

#define ALIGNMENT (8)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define BYTES_TO_HEADER_SIZE(bytes) ((uint32_t)((bytes) >> 3))
#define HEADER_SIZE_TO_BYTES(size)  ((uint32_t)((size) << 3))

#define INVALID_DATA_OFFSET ((uint32_t)-1)
#define GET_INDEX(header,mem) ((uint8_t*)(header)-(uint8_t*)(mem))

/* The Universal 8-byte Header */
typedef struct {
    uint32_t size:28,          /* Size in 8-byte multiples */
             is_allocated:1,   /* 1 if live, 0 if free */
             before_alloc:1,   /* 1 if previous contiguous block is live */
             custom_flags:2;   /* Generations for Nursery, ignored by Long-Lived */
             
    Handle handle;             /* The 4-byte packed Handle */
} BaseHeader;

/* 
 * The Universal Footer 
 * Placed at the very end of a block. Stores the total bytes of the block 
 * to allow O(1) reverse coalescing. 
 */
typedef uint32_t BaseFooter;
#define PUT_FOOTER(header) (*(BaseFooter*)((uint8_t*)(header) + HEADER_SIZE_TO_BYTES((header)->size) - sizeof(BaseFooter))= HEADER_SIZE_TO_BYTES((header)->size));

#endif