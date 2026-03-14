#ifndef STRATEGY_SLAB_H
#define STRATEGY_SLAB_H
typedef struct
{
    char stratigy_id;
    void* heap_start;
    uint32_t object_size;
    void* free_list;
} SlabAllocator;
void slab_init(SlabAllocator *s, uint32_t object_size);
void* slab_alloc(SlabAllocator *s);
void slab_free(SlabAllocator *s);
void slab_destroy(SlabAllocator *s);
#endif