#include "Interface/mem_manager.h"
#include "Arenas/handle.h" 
#include "Strategies/nursery.h"
#include <string.h>


#define PAGE_SIZE (4096)

static int initialized = 0;

int mm_init() {
    if (initialized) return 0;

    handle_table_init(PAGE_SIZE/sizeof(HandleEntry));
    
    initialized = 1;
    return 1;
}

void mm_destroy()
{
    
}
alloc_type_t mm_type_selector(uint32_t size)
{
    (void)size;
    return ALLOC_TYPE_NURSERY;
}

Handle mm_malloc(uint32_t size) {
    Handle invalid_handle = {INVALID_INDEX, 0};

    if ((!initialized && !mm_init())||size == 0) {
        return invalid_handle;
    }

    alloc_type_t type = mm_type_selector(size);
    Handle handle = handle_table_new(size, type);
    if (handle.index == INVALID_INDEX) return invalid_handle;
    if(type == ALLOC_TYPE_NURSERY)
    {
        if(nursery_malloc(size,handle)!=INVALID_DATA_OFFSET)
        {
            return handle;
        }
    }
    handle_table_free(handle);
    return invalid_handle;
}

void* mm_get_ptr(Handle handle) {
    if (!initialized) return NULL;

    return handle_table_get_ptr(handle);
}

void mm_free(Handle handle) {
    if (!initialized) return;

    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return;

    switch (entry->stratigy_id)
    {
        case ALLOC_TYPE_NURSERY:
            nursery_free(entry->data.data_ptr.data_offset);
            break;
        case ALLOC_TYPE_GENERAL:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_HUGE:
            break;
        default:
            break;
    }

    handle_table_free(handle);
}
Handle mm_realloc(Handle handle, uint32_t new_size)
{
    Handle invalid_handle = {INVALID_INDEX, 0};

    if (!initialized && !mm_init()) {
        return invalid_handle;
    }

    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return invalid_handle;

    alloc_type_t type = entry->stratigy_id;
    switch (type)
    {
        case ALLOC_TYPE_NURSERY:
            if(nursery_realloc(new_size,handle)!=INVALID_DATA_OFFSET)
            {
                entry->size = new_size;
                return handle;
            }
            break;
        case ALLOC_TYPE_GENERAL:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_HUGE:
            break;
        default:
            break;
    }
    return invalid_handle;
}
Handle mm_calloc(uint32_t size) {
    Handle invalid_handle = {INVALID_INDEX, 0};
    Handle handle = mm_malloc(size);
    if (handle.index == INVALID_INDEX) {
        return handle;
    }

    HandleEntry* entry = handle_table_get_entry(handle);
    if (!entry) {
        return invalid_handle;
    }
    alloc_type_t type = entry->stratigy_id;
    void* ptr = NULL;
    switch (type)
    {
        case ALLOC_TYPE_NURSERY:
            ptr = nursery_get(entry);
            break;
        case ALLOC_TYPE_GENERAL:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_HUGE:
            break;
        default:
            break;
    }
    if(ptr)
        memset(ptr,0,size);
    return handle;

}