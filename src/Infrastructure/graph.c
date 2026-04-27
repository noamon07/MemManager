#include "Infrastructure/graph.h"
#include "Infrastructure/handle.h"
#include "Infrastructure/scc_finder.h"
#include "Interface/memory_manager_priv.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INITIAL_EDGE_CAPACITY 1024
#define EDGE_CAPACITY_GROWTH  1024

Slab edges_slab;
void* edges_memory_base;

int expand_edges_memory() {
    uint32_t growth_bytes = EDGE_CAPACITY_GROWTH * sizeof(Edge);
    uint32_t new_size = edges_slab.slab_size + growth_bytes;
    uint8_t* new_base = (uint8_t*)mm_resize_region(edges_memory_base, edges_slab.slab_size, new_size, edges_slab.max_allowed_size);
    if (!new_base) {
        return 0;
    }

    edges_memory_base = new_base;
    slab_change_region(&edges_slab, new_size, edges_memory_base);
    
    return 1;
}


Slab* graph_init(uint32_t max_allowed_size) {
    slab_init(&edges_slab, sizeof(Edge));
    edges_slab.max_allowed_size = max_allowed_size;
    if(expand_edges_memory())
        return &edges_slab;
    return NULL;
}
void graph_destroy()
{
    if (edges_memory_base)
    {
        free(edges_memory_base);
        edges_memory_base = NULL;
    }
}


int graph_add_ref(Handle parent_handle, Handle child_handle)
{
    HandleEntry* parent_entry = handle_table_get_entry(parent_handle);
    HandleEntry* child_entry = handle_table_get_entry(child_handle);
    if(!parent_entry || !child_entry)
        return 0;

    uint32_t new_edge_offset = slab_alloc(&edges_slab, edges_memory_base);
    
    if (new_edge_offset == INVALID_DATA_OFFSET) {
        if (!expand_edges_memory()) return 0; 
        new_edge_offset = slab_alloc(&edges_slab, edges_memory_base);
    }

    Edge* edge = (Edge*)(edges_memory_base + new_edge_offset);

    edge->child_handle = child_handle;

    edge->next_edge_offset = parent_entry->first_edge_offset;
    parent_entry->first_edge_offset = new_edge_offset;
    child_entry->in_degree++;
    notify_edge_added(parent_handle, child_handle);
    return 1;
}
Handle get_scc_root(Handle node) {
    HandleEntry* entry = handle_table_get_entry(node);
    if (!entry) return INVALID_HANDLE;
    return entry->is_scc_root ? node : entry->scc.root_scc;
}
int graph_in_same_scc(Handle handle_a, Handle handle_b)
{
    Handle root_a = get_scc_root(handle_a);
    Handle root_b = get_scc_root(handle_b);
    if(!is_valid_handle(root_a) || !is_valid_handle(root_b))
        return 0;
    return handles_equal(root_a,root_b);
}
void graph_free_scc(Handle handle)
{
    Handle root = get_scc_root(handle);
    HandleEntry* root_entry = handle_table_get_entry(root);
    
    if(!root_entry) {
        return;
    }
        
    Handle cur = root;
    int keep_freeing = 1;
    
    while(!handles_equal(cur, INVALID_HANDLE) && keep_freeing)
    {
        HandleEntry* cur_entry = handle_table_get_entry(cur);
        
        if(cur_entry) {
            Handle next = cur_entry->next_in_scc;
            mm_free(cur);
            cur = next;
        } else {
            keep_freeing = 0; 
        }
    }
}
void update_scc_root_remove_edge(Handle parent_handle, Handle child_handle)
{
    if(graph_in_same_scc(parent_handle, child_handle))
    {
        Handle scc_root = get_scc_root(child_handle);
        HandleEntry* root_entry = handle_table_get_entry(scc_root);
        if (!root_entry || root_entry->scc.external_in_degree == 0) {
            return; 
        }
        recalculate_scc_subgraph(parent_handle);
    }
    else
    {
        Handle scc_root = get_scc_root(child_handle);
        HandleEntry* root_entry = handle_table_get_entry(scc_root);
        if(!root_entry) return;
        
        root_entry->scc.external_in_degree--;
        
        if(root_entry->scc.external_in_degree == 0)
        {
            graph_free_scc(scc_root);
        }
    }
}
void graph_dec_in_degree(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    if(entry)
    {
        entry->in_degree--;
    }
}
int graph_remove_ref(Handle parent_handle, Handle child_handle) {
    HandleEntry* parent_entry = handle_table_get_entry(parent_handle);
    HandleEntry* child_entry = handle_table_get_entry(child_handle);
    if(!parent_entry || !child_entry)
        return 0;
    uint32_t current_offset = parent_entry->first_edge_offset;
    uint32_t prev_offset = INVALID_DATA_OFFSET;

    while (current_offset != INVALID_DATA_OFFSET) {
        Edge* current_edge = (Edge*)(edges_memory_base + current_offset);
        if(handles_equal(current_edge->child_handle, child_handle))
        {
            if (prev_offset == INVALID_DATA_OFFSET) {
                parent_entry->first_edge_offset = current_edge->next_edge_offset;
            } 
            else
            {
                Edge* prev_edge = (Edge*)(edges_memory_base + prev_offset);
                prev_edge->next_edge_offset = current_edge->next_edge_offset;
            }
            graph_dec_in_degree(child_handle);
            update_scc_root_remove_edge(parent_handle, child_handle);

            slab_free(&edges_slab, current_offset, edges_memory_base);
            return 1;
        }

        prev_offset = current_offset;
        current_offset = current_edge->next_edge_offset;
    }

    return 0;
}

void graph_free(Handle parent_handle)
{
    HandleEntry* parent_entry = handle_table_get_entry(parent_handle);
    if(!parent_entry)
        return;
    uint32_t current_offset = parent_entry->first_edge_offset;

    while (current_offset != INVALID_DATA_OFFSET) {
        Edge* current_edge = (Edge*)(edges_memory_base + current_offset);
        Handle child_handle = current_edge->child_handle;
        uint32_t next_offset = current_edge->next_edge_offset;
        HandleEntry* child_entry = handle_table_get_entry(child_handle);
        if(child_entry)
        {
            graph_remove_ref(parent_handle, child_handle);
        }

        current_offset = next_offset;
    }
}
Edge graph_get_edge(uint32_t offset)
{
    if (offset == INVALID_DATA_OFFSET) {
        return INVALID_EDGE;
    }
    return *(Edge*)(edges_memory_base + offset);
}
Edge graph_get_first_edge(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return INVALID_EDGE;
    return graph_get_edge(entry->first_edge_offset);
}