#include "Arenas/promotion.h"
#include <string.h>
#include "Arenas/nursery.h"
#include "Arenas/general.h"


int nursury_promotion(uint32_t entry_index)
{
    // data_pos data;
    HandleEntry* entry = handle_table_get_entry_by_index(entry_index);
    uint32_t ptr_size = entry->size;
    if(!entry) return 0;
    uint32_t* entry_ptr = NULL;
    uint32_t new_offset = mm_malloc_general(ptr_size, &entry_ptr);
    if(new_offset==INVALID_GENERAL_INDEX) return 0;
    *entry_ptr = entry_index;

    data_pos old_data = entry->data.data_ptr;
    void* ptr = mm_nursery_get(entry);
    entry->data.data_ptr.data_offset = new_offset;
    entry->stratigy_id = ALLOC_TYPE_GENERAL;
    void *new_ptr = mm_general_get(entry);
    memmove(new_ptr,ptr,ptr_size);
    mm_free_nursery(old_data);
    return 1;
}