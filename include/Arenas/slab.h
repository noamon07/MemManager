#ifndef SLAB_H
#define SLAB_H

#include "arena.h"
#include <stddef.h>
#include <stdint.h>

#define MIN_SLAB_OBJ_SIZE sizeof(uint32_t)


typedef struct {
    uint32_t object_size;
    uint32_t free_list_head;
    
    uint32_t slab_size;
    uint32_t max_allowed_size;
} Slab;


void slab_init(Slab* slab, uint32_t obj_size);

uint32_t slab_alloc(Slab* slab, void* memory_base);

void slab_free(Slab* slab, uint32_t offset, void* memory_base);

void slab_change_region(Slab* slab, uint32_t region_size, void* memory_base);

#endif // SLAB_H