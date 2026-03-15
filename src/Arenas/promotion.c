#include "Arenas/promotion.h"
#include <string.h>
#include "Arenas/nursery.h"
#include "Arenas/general.h"


void nursury_promotion(HandleEntry* entry, uint32_t ptr_size)
{
    alloc_type_t type;
    void* new_ptr = mm_general_alloc(ptr_size, &type);
    if(!new_ptr) return;
    void* ptr = mm_nursery_get(entry);
    memmove(new_ptr,ptr,ptr_size);
    mm_free_nursery(entry->data.data_ptr);

    entry->data.data_ptr.ptr = new_ptr;
    entry->stratigy_id = type;
}