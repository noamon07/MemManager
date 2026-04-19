#include "Interface/mem_manager.h"
#include "Interface/memory_manager_priv.h"
#include "Infrastructure/handle.h" 
#include "Strategies/nursery.h"
#include "Infrastructure/graph.h"
#include "Infrastructure/scc_finder.h"
#include <string.h>
#include <stdlib.h>


#define PAGE_SIZE (4096)
typedef struct {
    size_t max_size;
    size_t current_usage;
} MemoryManager;
static MemoryManager manager;

int mm_init(size_t max_size)
{
    if (max_size == 0 || max_size < PAGE_SIZE || !graph_init()) return 0;
    manager.max_size = max_size;
    handle_table_init(PAGE_SIZE/sizeof(HandleEntry));
    manager.current_usage = PAGE_SIZE;
    return 1;
}

void mm_destroy()
{
    if(!manager.max_size)
        return;
    handle_table_destroy();
    memset(&manager,0,sizeof(MemoryManager));
}


Handle mm_malloc(size_t size) {
    if (!manager.max_size||size == 0) {
        return INVALID_HANDLE;
    }

    Handle handle = handle_table_new(size);
    if (!is_valid_handle(handle)) return INVALID_HANDLE;
    if(nursery_malloc(size,handle)!=INVALID_DATA_OFFSET)
    {
        HandleEntry* entry = handle_table_get_entry(handle);
        if(!entry)
            return INVALID_HANDLE;
        entry->size = size;
        return handle;
    }
    handle_table_free(handle);
    return INVALID_HANDLE;
}

void* mm_get_ptr(Handle handle) {
    if (!manager.max_size) return NULL;

    return handle_table_get_ptr(handle);
}
void mm_free(Handle handle) {
    if (!manager.max_size) return;

    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return;

    entry->in_degree = 0; 
    if(entry->strategy) {
        entry->strategy->free(entry->data.data_offset);
    }

    graph_free(handle);
    handle_table_free(handle);
}
Handle mm_realloc(Handle handle, size_t new_size)
{
    if (!manager.max_size) {
        return INVALID_HANDLE;
    }

    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return INVALID_HANDLE;

    if(entry->strategy)
    {
        if(entry->strategy->realloc(new_size,handle)!=INVALID_DATA_OFFSET)
        {
            entry->size = new_size;
            return handle;
        }
    }
    return INVALID_HANDLE;
}
Handle mm_calloc(size_t size) {
    Handle handle = mm_malloc(size);
    if (!is_valid_handle(handle)) {
        return INVALID_HANDLE;
    }

    HandleEntry* entry = handle_table_get_entry(handle);
    if (!entry) {
        return INVALID_HANDLE;
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
void mem_set_ref(Handle parent, Handle child)
{
    graph_add_ref(parent,child);
}
void mem_remove_ref(Handle parent, Handle child)
{
    graph_remove_ref(parent,child);
    HandleEntry* entry = handle_table_get_entry(child);
    if(!entry || entry->in_degree)
        return;
    mm_free(child);
}
void* mm_request_region(size_t size)
{
    if (manager.current_usage + size > manager.max_size) {
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (ptr) {
        manager.current_usage += size;
    }
    return ptr;
}
void* mm_resize_region(void* old_ptr, size_t old_size, size_t new_size)
{
    if (new_size == old_size) return old_ptr;

    if (new_size > old_size) {
        uint32_t growth = new_size - old_size;
        if (manager.current_usage + growth > manager.max_size) {
            return NULL; 
        }

        void* new_ptr = realloc(old_ptr, new_size);
        if (new_ptr) {
            manager.current_usage += growth;
        }
        return new_ptr;
    }
    else {
        uint32_t shrinkage = old_size - new_size;
        void* new_ptr = realloc(old_ptr, new_size);
        
        if (new_ptr) {
            manager.current_usage -= shrinkage;
            return new_ptr;
        }
        return old_ptr;
    }
    
}
Cursor mem_get_cursor(Handle handle)
{
    return (Cursor){handle, 0};
}
void _internal_write_handle(Cursor cur, void* ptr) {
    (void)cur;
    (void)ptr;
}

void _internal_write_data(Cursor cur, void* ptr, uint32_t size) {
    uint8_t* payload = mm_get_ptr(cur.handle);
    if (!payload) return;

    memcpy(payload + cur.byte_offset, ptr, size);
}
int handles_equal(Handle a, Handle b)
{
    return a.index == b.index && a.generation == b.generation;
}
int is_valid_handle(Handle h) 
{
    HandleEntry* entry = handle_table_get_entry(h);
    return entry != NULL;
}