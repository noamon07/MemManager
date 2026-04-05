#include "Infrastructure/scc_finder.h"
#include "Arenas/stack.h"
#include "Infrastructure/graph.h"
#include "Infrastructure/handle.h"
#include <stdlib.h>
#include <string.h>



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

// ==========================================
// THE CORE ALGORITHM
// ==========================================
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
                lowlinks[handle.index] = lowlinks[next_handle.index] < lowlinks[handle.index] ? lowlinks[next_handle.index] : lowlinks[handle.index];
            }
        }
        else
        {
            stack_pop(&call_stack, frame);
            handle = frame->Parent_handle;
            TarjanFrame* parent_frame;
            stack_top(&call_stack, (void**)&parent_frame);
            if(lowlinks[handle.index] < lowlinks[parent_frame->Parent_handle.index])
            {
                lowlinks[parent_frame->Parent_handle.index] = lowlinks[handle.index];
            }
            else if(discovery[handle.index] == lowlinks[handle.index])
            {
                Handle popped_handle;
                do
                {
                    stack_pop(&scc_stack, &popped_handle);
                    entry = handle_table_get_entry(popped_handle);
                    entry->is_on_scc_stack = 0;
                } while (handle.index != popped_handle.index);
            }
        }
    }
}
void dfs(Handle handle)
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