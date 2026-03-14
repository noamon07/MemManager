#include "Interface/mem_manager.h"
#include "strategies/slab.h" 
#include "Arenas/handle.h" 
#include "Arenas/huge.h"

#define PAGE_SIZE (4096)

static int initialized = 0;
static HandleTable table;

int mm_init() {
    if (initialized) return 0;

    handle_table_init(&table, PAGE_SIZE/sizeof(HandleEntry));
    
    initialized = 1;
    return 1;
}

void mm_destroy()
{
    
}
alloc_type_t mm_type_selector(uint32_t size)
{
    if(size >= PAGE_SIZE)
    {
        return ALLOC_TYPE_HUGE;
    }
    return ALLOC_TYPE_ERROR;
}

Handle mm_malloc(uint32_t size) {
    Handle h = {INVALID_INDEX, 0};

    if (!initialized) {
        if (!mm_init()) return h;
    }

    alloc_type_t type = mm_type_selector(size);
    void* ptr = NULL;
    switch (type)
    {
        case ALLOC_TYPE_TLSF:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_BUMP:
            break;
        case ALLOC_TYPE_HUGE:
            ptr = mm_malloc_huge(size);
            break;
        default:
            break;
    }
    if(ptr)
    {
        h = handle_table_new(&table, ptr, size,type);
    }
    return h;
}

void* mm_get_ptr(Handle handle) {
    if (!initialized) return NULL;

    return handle_table_get_ptr(&table, handle);
}

void mm_free(Handle handle) {
    if (!initialized) return;

    HandleEntry* entry = handle_table_get_entry(&table, handle);
    switch (entry->stratigy_id)
    {
        case ALLOC_TYPE_TLSF:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_BUMP:
            break;
        case ALLOC_TYPE_HUGE:
            mm_free_huge(entry->data.ptr, entry->size);
            break;
        default:
            break;
    }

    handle_table_free(&table, handle);
}
Handle mm_realloc(Handle handle, uint32_t new_size)
{
    Handle h = {INVALID_INDEX, 0};

    if (!initialized) {
        if (!mm_init()) return h;
    }

    HandleEntry* entry = handle_table_get_entry(&table, handle);
    alloc_type_t type = entry->stratigy_id;
    void* ptr_old= entry->data.ptr;
    void* ptr = NULL;
    switch (type)
    {
        case ALLOC_TYPE_TLSF:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_BUMP:
            break;
        case ALLOC_TYPE_HUGE:
            ptr = mm_realloc_huge(ptr_old,entry->size,new_size);
            break;
        default:
            break;
    }
    if(ptr)
    {
        entry->data.ptr = ptr;
        entry->size = new_size;
        h= handle;
    }
    return h;
}
Handle mm_calloc(uint32_t size)
{
    Handle h = {INVALID_INDEX, 0};

    if (!initialized) {
        if (!mm_init()) return h;
    }

    alloc_type_t type = mm_type_selector(size);
    void* ptr = NULL;
    switch (type)
    {
        case ALLOC_TYPE_TLSF:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_BUMP:
            break;
        case ALLOC_TYPE_HUGE:
            ptr = mm_calloc_huge(size);
            break;
        default:
            break;
    }
    if(ptr)
    {
        h = handle_table_new(&table, ptr, size,type);
    }
    return h;
}