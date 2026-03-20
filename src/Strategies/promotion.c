#include "Strategies/promotion.h"
#include <string.h>
#include "Strategies/nursery.h"


int nursury_promotion(Handle handle)
{
    (void)handle;
    return 0;

    // data_pos data;
    // HandleEntry* entry = handle_table_get_entry(handle
    // HandleEntry* entry = handle_table_get_entry(handle);
    // uint32_t ptr_size = entry->size;
    // if(!entry) return 0;
    // uint32_t new_offset = mm_malloc_general(ptr_size, handle);
    // if(new_offset==INVALID_DATA_OFFSET) return 0;
    // *entry_ptr = entry_index;

    // data_pos old_data = entry->data.data_ptr;
    // void* ptr = mm_nursery_get(entry);
    // entry->data.data_ptr.data_offset = new_offset;
    // entry->stratigy_id = ALLOC_TYPE_GENERAL;
    // void *new_ptr = mm_general_get(entry);
    // memmove(new_ptr,ptr,ptr_size);
    // mm_free_nursery(old_data);
    // return 1;
}