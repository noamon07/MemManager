#ifndef NURSERY_H
#define NURSERY_H

#include <stdint.h>
#include "strategies/bump.h"


typedef struct NurseryNode {
    BumpAllocator* allocator;   /* Pointer to your actual bump allocator state */
    struct NurseryNode* next;   /* Next node in the chain */
    size_t total_mapped_size;
} NurseryNode;

typedef struct Nursery {
    NurseryNode* head;          /* The first bump allocator in the chain */
    NurseryNode* active;        /* The bump allocator currently being used */
    size_t default_block_size;  /* Size to pass when creating a new bump allocator */
} Nursery;

/* Initialization and teardown */
void nursery_init(Nursery* nursery, size_t default_block_size);
void nursery_destroy(Nursery* nursery);

/* * Clears the nursery after a GC pass.
 * Instead of freeing memory, this should just iterate through the chain 
 * and call your bump_reset() function on each allocator, then point 
 * the 'active' node back to the 'head'.
 */
void nursery_reset(Nursery* nursery);

/* Internal slow path: handles expanding the chain when the active block is full */
void* nursery_allocate_slow_path(Nursery* nursery, size_t size);

/*
 * The main allocation function.
 * We keep this static inline to maintain that O(1) performance.
 * * NOTE: This assumes your bump_allocate function returns NULL 
 * when it doesn't have enough space left.
 */
void* nursery_allocate(Nursery* nursery, size_t size);

#endif