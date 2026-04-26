#include "Interface/mem_manager.h"
#include "Interface/memory_manager_priv.h"
#include "Infrastructure/handle.h" 
#include "Strategies/nursery.h"
#include "Infrastructure/graph.h"
#include "Infrastructure/scc_finder.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Arenas/slab.h"
#include "Strategies/general.h"


#define PAGE_SIZE (4096)
#define MAX_SUSPECT_NODES (1024)

typedef struct {
    uint32_t max_size;
    uint32_t current_usage;
    HandleTable* handle_table;
    Slab* graph;
    General* general;
    Nursery* nursery;
    Scc_Finder* scc_finder;
} MemoryManager;
static MemoryManager manager;

int mm_init(MemoryManagerConfig config)
{
    manager.max_size = config.total_system_memory;
    uint32_t handle_table_size = config.max_handles * sizeof(HandleEntry);
    uint32_t scc_finder_size = config.max_handles * (sizeof(TarjanFrame) + sizeof(Handle) + sizeof(NodeTimes));
    uint32_t edges_count = (uint32_t)(config.max_handles * config.expected_graph_density);
    uint32_t edges_slab_size = edges_count * sizeof(Edge);
    uint32_t total_metadata = handle_table_size + scc_finder_size + edges_slab_size;
    if (total_metadata >= config.total_system_memory) {
        printf("INIT ERROR: Metadata overhead (%zu) exceeds total memory (%zu)!\n", 
                (size_t)total_metadata, (size_t)config.total_system_memory);
        return 0;
    }
    uint32_t remaining_data_space = config.total_system_memory - total_metadata;
    uint32_t nursery_size = (uint32_t)(remaining_data_space * config.nursery_ratio);
    uint32_t general_size = remaining_data_space - nursery_size;
    if(nursery_size<PAGE_SIZE || general_size<PAGE_SIZE)
        return 0;
    manager.handle_table = handle_table_init(PAGE_SIZE/sizeof(HandleEntry),handle_table_size);
    manager.scc_finder = scc_finder_init(manager.handle_table->size,scc_finder_size);
    manager.graph = graph_init(edges_slab_size);
    
    manager.general = general_init(PAGE_SIZE,general_size);
    manager.nursery = nursery_init(PAGE_SIZE,nursery_size);
    manager.current_usage = manager.handle_table->size * sizeof(HandleEntry) +
                            manager.scc_finder->size +
                            manager.graph->slab_size +
                            manager.general->mem_size +
                            manager.nursery->bump.mem_size;
    return 1;
}

void mm_destroy()
{
    if(!manager.max_size)
        return;
    handle_table_destroy();
    general_destroy(manager.general);
    nursery_destroy(manager.nursery);
    scc_finder_destroy(manager.scc_finder);
    graph_destroy();
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
void* mm_resize_region(void* old_ptr, size_t old_size, size_t new_size, uint32_t max_allowed_size)
{
    if (new_size == old_size) return old_ptr;

    if (new_size > old_size) {
        uint32_t growth = new_size - old_size;
        if (manager.current_usage + growth > manager.max_size || new_size > max_allowed_size) {
            printf("GOT TO SIZE LIMIT!\n");
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
int handles_equal(Handle a, Handle b)
{
    return a.index == b.index && a.generation == b.generation;
}
int is_valid_handle(Handle h) 
{
    HandleEntry* entry = handle_table_get_entry(h);
    return entry != NULL;
}
void write_handle(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return;
    entry->in_degree++;
    Handle root = get_scc_root(handle);
    HandleEntry* root_entry = handle_table_get_entry(root);
    if(!root_entry)
        return;
    root_entry->scc.external_in_degree++;
}
void clear_handle(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry) return;
    
    // 1. Drop the raw incoming degree
    entry->in_degree--;
    
    // 2. Drop the external anchor on the cluster
    Handle root = get_scc_root(handle);
    HandleEntry* root_entry = handle_table_get_entry(root);
    if(!root_entry) return;
    
    root_entry->scc.external_in_degree--;
    
    // 3. If the cluster just lost its last anchor to reality, burn it down!
    if (root_entry->scc.external_in_degree == 0) {
        evaluate_scc_viability(root);
    }
}