#include "Arenas/slab.h"
#include <stdio.h>


void slab_init(Slab* slab, uint32_t obj_size)
{
    if (obj_size < MIN_SLAB_OBJ_SIZE) {
        obj_size = MIN_SLAB_OBJ_SIZE;
    }
    slab->object_size = ALIGN(obj_size);
    slab->free_list_head = INVALID_DATA_OFFSET;
    slab->slab_size = 0;
}
void slab_change_region(Slab* slab, uint32_t new_region_size, void* memory_base)
{
    if (new_region_size <= slab->slab_size) return;
    void* slot_ptr = memory_base + (new_region_size - slab->object_size);
    // Iterate backwards through the NEW slots so the lowest index becomes the head
    for (; slot_ptr >= memory_base + slab->slab_size; slot_ptr = (void*)slot_ptr - slab->object_size)
    {
        *(uint32_t*)slot_ptr = slab->free_list_head;
        
        slab->free_list_head = slot_ptr - memory_base;
    }
    
    slab->slab_size = new_region_size;
}
uint32_t slab_alloc(Slab* slab, void* memory_base)
{
    if (slab->free_list_head == INVALID_DATA_OFFSET) {
        return INVALID_DATA_OFFSET;
    }

    uint32_t allocated_offset = slab->free_list_head;
    
    slab->free_list_head = *(uint32_t*)(memory_base + allocated_offset);

    return allocated_offset;
}
void slab_free(Slab* slab, uint32_t offset, void* memory_base)
{
    if (offset == INVALID_DATA_OFFSET || offset >= slab->slab_size) return;

    uint32_t* slot_ptr = (uint32_t*)(memory_base + offset);

    *slot_ptr = slab->free_list_head;

    slab->free_list_head = offset;
}
