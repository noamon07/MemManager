#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>
/* Included so we know what a handle ID is, assuming your data_pos is here too */
#include "Arenas/handle.h" 

/* 1. Global Alignment Rules */
#define ALIGNMENT (8)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* 2. Universal Size Conversions (Assuming 8-byte alignment saves 3 bits) */
#define HEADER_SIZE_TO_BYTES(header_size) ((uint32_t)((header_size) << 3))
#define BYTES_TO_HEADER_SIZE(bytes) ((uint32_t)((bytes) >> 3))

/* 3. The Universal Header 
 * Every allocator (Nursery, Long-Lived, Bump, TLSF) MUST agree on this.
 * It must be the first field in any custom struct you build.
 */
typedef struct {
    uint32_t size:28,          /* Size in 8-byte multiples */
             is_allocated:1,   /* 1 if live, 0 if free */
             before_alloc:1,   /* 1 if previous contiguous block is live */
             custom_flags:2;   /* Let the specific arena decide what these mean (e.g., generation) */
             
    Handle handle;      /* The Handle ID mapped to this block. Crucial for moving memory. */
} BaseHeader;

#endif