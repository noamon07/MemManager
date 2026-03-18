#include "Arenas/promotion.h"
#include <string.h>
#include "Arenas/nursery.h"
#include "Arenas/general.h"


void nursury_promotion(uint32_t entry_index, uint32_t ptr_size)
{
    data_pos data;
    HandleEntry* entry = handle_table_get_entry_by_index(entry_index);
    if(!entry) return;
    uint32_t* entry_index_ptr = &entry_index;
    data.data_offset = mm_malloc_general(ptr_size, &entry_index_ptr);
    if(data.data_offset!=INVALID_GENERAL_INDEX) return;
    void* ptr = mm_nursery_get(entry);
    void *new_ptr = mm_general_get(entry);
    memmove(new_ptr,ptr,ptr_size);
    mm_free_nursery(entry->data.data_ptr);

    entry->data.data_ptr.data_offset = data.data_offset;
    entry->stratigy_id = ALLOC_TYPE_GENERAL;
}