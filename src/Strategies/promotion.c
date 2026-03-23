#include "Strategies/promotion.h"
#include <string.h>
#include "Strategies/nursery.h"
#include "Strategies/general.h"


int nursury_promotion(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    void* ptr = NULL;
    if(!entry || !(ptr = entry->strategy->get(entry->data.data_ptr.data_offset))) return 1;
    uint32_t ptr_size = entry->size;
    uint32_t new_offset = general_malloc(ptr_size, handle);
    if(new_offset==INVALID_DATA_OFFSET) return 0;
    
    void *new_ptr = entry->strategy->get(new_offset);
    memmove(new_ptr,ptr,ptr_size);
    return 1;
}