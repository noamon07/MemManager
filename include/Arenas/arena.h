#ifndef ARENA_H
#define ARENA_H

#include <stdint.h>
#include "Interface/mem_manager.h"
#include "Infrastructure/handle.h"

#define ALIGNMENT (8)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define BYTES_TO_HEADER_SIZE(bytes) ((uint32_t)((bytes) >> 3))
#define HEADER_SIZE_TO_BYTES(size)  ((uint32_t)((size) << 3))

#define INVALID_DATA_OFFSET ((uint32_t)-1)
#define GET_INDEX(header,mem) ((uint8_t*)(header)-(uint8_t*)(mem))

typedef struct {
    uint32_t size:28,          /* Block size in 8-byte words (max 2GB) */
             is_allocated:1,   /* 1 if block is in use, 0 if free */
             before_alloc:1,   /* 1 if previous contiguous block is allocated */
             custom_flags:2;   /* Reserved for internal GC/State markers */
             
    Handle handle;             /* Bitfield-based struct (Index + Generation) */
} BaseHeader;

typedef uint32_t BaseFooter;
#define PUT_FOOTER(header) (*(BaseFooter*)((uint8_t*)(header) + HEADER_SIZE_TO_BYTES((header)->size) - sizeof(BaseFooter))= HEADER_SIZE_TO_BYTES((header)->size));

#endif