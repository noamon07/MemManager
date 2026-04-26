#ifndef NURSERY_H
#define NURSERY_H

#include "Arenas/arena.h"
#include "Arenas/bump.h"

#define NURSERY_PROMOTION_GENERATION 3

typedef struct {
    BumpAllocator bump;
} Nursery;

Nursery* nursery_init(uint32_t initial_size, uint32_t max_allowed_size);
void nursery_destroy();
uint32_t nursery_malloc(uint32_t size, Handle handle);
uint32_t nursery_realloc(uint32_t new_size, Handle handle);
void nursery_free(uint32_t offset);
void* nursery_get(uint32_t offset);
Nursery* get_nursery_instance(void);

#endif
