#include "Infrastructure/graph.h"
#include "Arenas/slab.h"
#include "Infrastructure/handle.h" // To access HandleEntry
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

    void* new_base = realloc(edges_memory_base, new_size);
    if (!new_base) {
        return 0;
    }

    edges_memory_base = new_base;
    slab_change_region(&edges_slab, new_size, edges_memory_base);
    
    return 1;
}


int graph_init() {
    slab_init(&edges_slab, sizeof(Edge));
    return expand_edges_memory();
}

int graph_add_ref(Handle parent_handle, Handle child_handle)
{
    HandleEntry* parent_entry = handle_table_get_entry(parent_handle);
    HandleEntry* child_entry = handle_table_get_entry(child_handle);
    if(!parent_entry || !child_entry)
        return 0;

    uint32_t new_edge_offset = slab_alloc(&edges_slab, edges_memory_base);
    
    if (new_edge_offset == INVALID_INDEX) {
        if (!expand_edges_memory()) return 0; 
        new_edge_offset = slab_alloc(&edges_slab, edges_memory_base);
    }

    Edge* edge = (Edge*)(edges_memory_base + new_edge_offset);

    edge->child_handle = child_handle;

    edge->next_edge_offset = parent_entry->first_edge_offset;
    parent_entry->first_edge_offset = new_edge_offset;

    child_entry->in_degree++;

    return 1;
}

int graph_remove_ref(Handle parent_handle, Handle child_handle) {
    HandleEntry* parent_entry = handle_table_get_entry(parent_handle);
    HandleEntry* child_entry = handle_table_get_entry(child_handle);
    if(!parent_entry || !child_entry)
        return 0;
    uint32_t current_offset = parent_entry->first_edge_offset;
    uint32_t prev_offset = INVALID_INDEX;

    while (current_offset != INVALID_INDEX) {
        Edge* current_edge = (Edge*)(edges_memory_base + current_offset);

        if (!memcmp(&(current_edge->child_handle), &child_handle, sizeof(Handle))) {
            if (prev_offset == INVALID_INDEX) {
                parent_entry->first_edge_offset = current_edge->next_edge_offset;
            } else {
                Edge* prev_edge = (Edge*)(edges_memory_base + prev_offset);
                prev_edge->next_edge_offset = current_edge->next_edge_offset;
            }

            child_entry->in_degree--;

            slab_free(&edges_slab, current_offset, edges_memory_base);
            return 1;
        }

        prev_offset = current_offset;
        current_offset = current_edge->next_edge_offset;
    }

    return 0;
}

void graph_clear_all_refs(Handle parent_handle, FreeFunction free_fn)
{
    HandleEntry* parent_entry = handle_table_get_entry(parent_handle);
    if(!parent_entry)
        return;
    uint32_t current_offset = parent_entry->first_edge_offset;

    while (current_offset != INVALID_INDEX) {
        Edge* current_edge = (Edge*)(edges_memory_base + current_offset);
        Handle child = current_edge->child_handle;
        uint32_t next_offset = current_edge->next_edge_offset;
        HandleEntry* child_entry = handle_table_get_entry(child);
        if(child_entry && child_entry->in_degree > 0)
        {
            child_entry->in_degree--;
            if (child_entry->in_degree == 0 && free_fn != NULL) {
                free_fn(child);
            }
        }

        slab_free(&edges_slab, current_offset, edges_memory_base);

        current_offset = next_offset;
    }

    parent_entry->first_edge_offset = INVALID_INDEX;
}
Edge graph_get_edge(uint32_t offset)
{
    if (offset == INVALID_INDEX) {
        return (Edge){INVALID_HANDLE, INVALID_INDEX};
    }
    return *(Edge*)(edges_memory_base + offset);
}
Edge graph_get_first_edge(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return (Edge){INVALID_HANDLE, INVALID_INDEX};
    return graph_get_edge(entry->first_edge_offset);
}