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
    if(!initialized)
        return;
    handle_table_destroy();
    initialized = 0;
}


Handle mm_malloc(uint32_t size) {
    Handle invalid_handle = {INVALID_INDEX, 0};

    if ((!initialized && !mm_init())||size == 0) {
        return invalid_handle;
    }

    Handle handle = handle_table_new(size);
    if (handle.index == INVALID_INDEX) return invalid_handle;
    if(nursery_malloc(size,handle)!=INVALID_DATA_OFFSET)
    {
        HandleEntry* entry = handle_table_get_entry(handle);
        if(!entry)
            return invalid_handle;
        entry->size = size;
        return handle;
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

    entry->strategy->free(entry->data.data_offset);

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

    if(entry->strategy)
    {
        if(entry->strategy->realloc(new_size,handle)!=INVALID_DATA_OFFSET)
        {
            entry->size = new_size;
            return handle;
        }
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
    void* ptr = NULL;
    if(entry->strategy)
    {
        ptr = entry->strategy->get(entry->data.data_offset);
    }
    if(ptr)
        memset(ptr,0,size);
    return handle;

}