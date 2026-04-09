#include "Infrastructure/scc_finder.h"
#include "Arenas/stack.h"
#include "Infrastructure/graph.h"
#include "Infrastructure/handle.h"
#include <stdlib.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

typedef struct {
    Handle Parent_handle;
    Edge edge_cursor;
} TarjanFrame;

// Our pre-allocated data
static Stack call_stack;
static Stack scc_stack;
static uint32_t* discovery = NULL;
static uint32_t* lowlinks = NULL;

static uint32_t current_time = 1;

// ==========================================
// INITIALIZATION & CLEANUP
// ==========================================
int scc_finder_init(uint32_t max_suspect_nodes)
{    
    int fail = !stack_init(&call_stack, max_suspect_nodes, sizeof(TarjanFrame));
    fail |= !stack_init(&scc_stack, max_suspect_nodes, sizeof(Handle));
    discovery = (uint32_t*)calloc(max_suspect_nodes, sizeof(uint32_t));
    lowlinks = (uint32_t*)calloc(max_suspect_nodes, sizeof(uint32_t));
    fail |= !discovery || !lowlinks;
    if(fail)
    {
        scc_finder_destroy();
        return 0;
    }
    return 1;
}

void scc_finder_destroy()
{
    stack_destroy(&call_stack);
    stack_destroy(&scc_stack);
    free(discovery);
    free(lowlinks);
}
void set_up_root_scc(Handle root_handle)
{
    HandleEntry* root_entry = handle_table_get_entry(root_handle);
    if(!root_entry)
        return;
    uint32_t total_in_degree = 0;
    uint32_t internal_edges = 0;
    Handle current_handle = root_handle;
    HandleEntry* current_entry = root_entry;
    HandleEntry* current_edge_entry = NULL;
    while(current_handle.index != INVALID_INDEX)
    {
        current_entry = handle_table_get_entry(current_handle);
        if(current_entry)
        {
            total_in_degree+=current_entry->in_degree;
            Edge e = graph_get_first_edge(current_handle);
            while(e.child_handle.index != INVALID_INDEX) {
                current_edge_entry = handle_table_get_entry(e.child_handle);
                if(current_edge_entry && (current_edge_entry->is_scc_root || current_edge_entry->scc.root_scc.index == root_handle.index))
                {
                    internal_edges++;
                }
                e = graph_get_edge(e.next_edge_offset);
            }

            current_handle = current_entry->next_in_scc;
        }
    }
    root_entry->scc.external_in_degree = total_in_degree - internal_edges;
    if (root_entry->scc.external_in_degree == 0) 
    {
        Handle current_dead = root_handle;
        
        while (current_dead.index != INVALID_INDEX) 
        {
            HandleEntry* dead_entry = handle_table_get_entry(current_dead);
            
            graph_clear_all_refs(current_dead, mm_free);
            
            if (dead_entry->strategy && dead_entry->strategy->free) {
                dead_entry->strategy->free(dead_entry->data.data_offset);
            }
            Handle next = dead_entry->next_in_scc;
            handle_table_free(current_dead);
            current_dead = next;
        }
    }
}
void set_up_new_scc()
{
    Handle handle = INVALID_HANDLE;
    Handle root_handle = INVALID_HANDLE;
    Handle* parent_handle = NULL;
    HandleEntry* entry = NULL;
    int root_found = 0;
    
    if(stack_pop(&scc_stack, &root_handle))
    {
        entry = handle_table_get_entry(root_handle);
        if(entry)
        {
            entry->is_on_scc_stack = 0;
            entry->is_scc_root = 1;
            if(discovery[root_handle.index] == lowlinks[root_handle.index])
            {
                entry->next_in_scc = INVALID_HANDLE;
                root_found = 1;
            }
            else
            {
                parent_handle = &handle;
                stack_top(&scc_stack, (void**)&parent_handle);
                if(parent_handle)
                {
                    entry->next_in_scc = *parent_handle;
                }
            }
        }
    }
    while(!root_found && stack_pop(&scc_stack, &handle))
    {
        entry = handle_table_get_entry(handle);
        if(entry)
        {
            entry->is_on_scc_stack = 0;
            entry->is_scc_root = 0;
            entry->scc.root_scc = root_handle;
            if(discovery[handle.index] == lowlinks[handle.index])
            {
                entry->next_in_scc = INVALID_HANDLE;
                root_found = 1;
            }
            else
            {
                parent_handle = &handle;
                stack_top(&scc_stack, (void**)&parent_handle);
                entry = handle_table_get_entry(handle);
                if(entry)
                {
                    entry->next_in_scc = *parent_handle;
                }
            }
        }
        else
        {
            root_found = 1;
        }
    }
    set_up_root_scc(root_handle);
    
}
void dfs_visit(Handle handle)
{
    TarjanFrame* frame = NULL;
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return;
    discovery[handle.index] = ++current_time;
    lowlinks[handle.index] = current_time;
    Edge edge = graph_get_first_edge(handle);
    stack_push(&call_stack, &(TarjanFrame){handle, edge});
    stack_push(&scc_stack, &handle);
    entry->is_on_scc_stack = 1;

    while(call_stack.count!=0)
    {
        stack_top(&call_stack, (void**)&frame);
        handle = frame->Parent_handle;
        edge = frame->edge_cursor;
        if(edge.child_handle.index != INVALID_INDEX)
        {
            frame->edge_cursor = graph_get_edge(edge.next_edge_offset);
            Handle next_handle = edge.child_handle;
            entry = handle_table_get_entry(next_handle);
            if(!discovery[next_handle.index])
            {
                discovery[next_handle.index] = ++current_time;
                lowlinks[next_handle.index] = current_time;
                edge = graph_get_first_edge(next_handle);
                stack_push(&call_stack, &(TarjanFrame){next_handle, edge});
                stack_push(&scc_stack, &next_handle);
                entry->is_on_scc_stack = 1;
            }
            else if(entry->is_on_scc_stack)
            {
                lowlinks[handle.index] = MIN(lowlinks[handle.index], discovery[next_handle.index]);
            }
        }
        else
        {
            stack_pop(&call_stack, frame);
            handle = frame->Parent_handle;
            TarjanFrame* parent_frame;
            if(stack_top(&call_stack, (void**)&parent_frame) && lowlinks[handle.index] < lowlinks[parent_frame->Parent_handle.index])
            {
                lowlinks[parent_frame->Parent_handle.index] = lowlinks[handle.index];
            }
            if(discovery[handle.index] == lowlinks[handle.index])
            {
                set_up_new_scc();
            }
        }
    }
}
void scc_process_suspect(Handle handle)
{
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return;
    if(!entry->is_scc_root)
    {
        handle = entry->scc.root_scc;
        entry = handle_table_get_entry(handle);
    }
    current_time = 0;
    while(entry)
    {
        if(!discovery[handle.index])
        {
            dfs_visit(handle);
        }
        handle = entry->next_in_scc;
        entry = handle_table_get_entry(handle);
    }
}