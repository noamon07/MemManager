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

static Stack call_stack;
static Stack scc_stack;
static uint32_t* discovery = NULL;
static uint32_t* lowlinks = NULL;
static uint64_t current_time = 0;

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

void evaluate_scc_viability(Handle scc_representative)
{
    HandleEntry* root_entry = handle_table_get_entry(scc_representative);
    if(!root_entry) return;

    uint32_t total_in_degree = 0;
    uint32_t internal_edges = 0;
    Handle current_node = scc_representative;

    while(current_node.index != INVALID_INDEX)
    {
        HandleEntry* current_entry = handle_table_get_entry(current_node);
        if(current_entry)
        {
            total_in_degree += current_entry->in_degree;
            Edge current_edge = graph_get_first_edge(current_node);
            
            while(current_edge.child_handle.index != INVALID_INDEX) 
            {
                HandleEntry* child_entry = handle_table_get_entry(current_edge.child_handle);
                if (child_entry && 
                   ((child_entry->is_scc_root && current_edge.child_handle.index == scc_representative.index) || 
                    (!child_entry->is_scc_root && child_entry->scc.root_scc.index == scc_representative.index)))
                {
                    internal_edges++;
                }
                current_edge = graph_get_edge(current_edge.next_edge_offset);
            }
            current_node = current_entry->next_in_scc;
        }
        else
        {
            current_node.index = INVALID_INDEX;
        }
    }

    root_entry->scc.external_in_degree = total_in_degree - internal_edges;

    if (root_entry->scc.external_in_degree == 0) 
    {
        Handle dead_node = scc_representative;
        
        while (dead_node.index != INVALID_INDEX) 
        {
            HandleEntry* dead_entry = handle_table_get_entry(dead_node);
            if (dead_entry)
            {
                Handle next_dead = dead_entry->next_in_scc;
                
                graph_free(dead_node, mm_free);
                
                if (dead_entry->strategy && dead_entry->strategy->free) {
                    dead_entry->strategy->free(dead_entry->data.data_offset);
                }
                
                handle_table_free(dead_node);
                dead_node = next_dead;
            }
            else
            {
                dead_node.index = INVALID_INDEX; // Stop condition (no break needed)
            }
        }
    }
}
void extract_scc_from_stack()
{
    Handle current_node = INVALID_HANDLE;
    Handle scc_representative = INVALID_HANDLE;
    Handle* peeked_handle = NULL;
    
    int is_first_popped = 0;
    int is_root_found = 0;
    
    while(!is_root_found && stack_pop(&scc_stack, &current_node))
    {
        HandleEntry* entry = handle_table_get_entry(current_node);
        if(entry)
        {
            if (!is_first_popped)
            {
                scc_representative = current_node;
                entry->is_scc_root = 1;
                is_first_popped = 1;
            }
            else
            {
                entry->is_scc_root = 0;
                entry->scc.root_scc = scc_representative;
            }
            entry->is_on_scc_stack = 0;

            if(discovery[current_node.index] == lowlinks[current_node.index])
            {
                entry->next_in_scc.index = INVALID_INDEX;
                is_root_found = 1;
            }
            else
            {
                Handle ph;
                peeked_handle = &ph;
                if(stack_top(&scc_stack, (void**)&peeked_handle))
                {
                    entry->next_in_scc = *peeked_handle;
                }
                else
                {
                    entry->next_in_scc.index = INVALID_INDEX;
                }
            }
        }
        else
        {
            is_root_found = 1;
        }
    }
    
    evaluate_scc_viability(scc_representative);
}

void tarjan_dfs_visit(Handle start_node)
{
    HandleEntry* entry = handle_table_get_entry(start_node);
    if(!entry) return;

    discovery[start_node.index] = ++current_time;
    lowlinks[start_node.index] = current_time;
    
    Edge first_edge = graph_get_first_edge(start_node);
    TarjanFrame initial_frame = {start_node, first_edge};
    
    stack_push(&call_stack, &initial_frame);
    stack_push(&scc_stack, &start_node);
    entry->is_on_scc_stack = 1;

    int error_flag = 0;

    while(call_stack.count != 0 && !error_flag)
    {
        TarjanFrame* frame = NULL;
        if (!stack_top(&call_stack, (void**)&frame) || frame == NULL) 
        {
            error_flag = 1; 
        }
        else
        {
            Handle current_node = frame->Parent_handle;
            Edge current_edge = frame->edge_cursor;

            if(current_edge.child_handle.index != INVALID_INDEX)
            {
                frame->edge_cursor = graph_get_edge(current_edge.next_edge_offset);
                
                Handle child_node = current_edge.child_handle;
                HandleEntry* child_entry = handle_table_get_entry(child_node);
                
                if(!discovery[child_node.index])
                {
                    discovery[child_node.index] = ++current_time;
                    lowlinks[child_node.index] = current_time;
                    Edge child_edge = graph_get_first_edge(child_node);
                    
                    TarjanFrame child_frame = {child_node, child_edge};
                    stack_push(&call_stack, &child_frame);
                    stack_push(&scc_stack, &child_node);
                    
                    if (child_entry) child_entry->is_on_scc_stack = 1;
                }
                else if(child_entry && child_entry->is_on_scc_stack)
                {
                    lowlinks[current_node.index] = MIN(lowlinks[current_node.index], discovery[child_node.index]);
                }
            }
            else
            {
                TarjanFrame dummy; 
                stack_pop(&call_stack, &dummy);
                
                TarjanFrame* parent_frame = NULL;
                if(stack_top(&call_stack, (void**)&parent_frame))
                {
                    Handle parent_node = parent_frame->Parent_handle;
                    lowlinks[parent_node.index] = MIN(lowlinks[parent_node.index], lowlinks[current_node.index]);
                }
                
                if(discovery[current_node.index] == lowlinks[current_node.index])
                {
                    extract_scc_from_stack();
                }
            }
        }
    }
}
void recalculate_scc_subgraph(Handle start_handle)
{
    HandleEntry* entry = handle_table_get_entry(start_handle);
    if(!entry) return;

    if(!entry->is_scc_root && entry->scc.root_scc.index != INVALID_INDEX)
    {
        start_handle = entry->scc.root_scc;
        entry = handle_table_get_entry(start_handle);
        if (!entry) return;
    }
    
    Handle current_old = start_handle;
    uint32_t pending_count = 0;
    
    while(current_old.index != INVALID_INDEX)
    {
        HandleEntry* old_entry = handle_table_get_entry(current_old);
        if (old_entry)
        {
            Handle next_old = old_entry->next_in_scc;
            
            old_entry->is_scc_root = 0;
            old_entry->next_in_scc.index = INVALID_INDEX;
            discovery[current_old.index] = 0; 
            
            stack_push(&scc_stack, &current_old);
            pending_count++;
            
            current_old = next_old;
        }
        else
        {
            current_old.index = INVALID_INDEX;
        }
    }
        
    while(pending_count > 0)
    {
        Handle member_to_process;
        if (stack_pop(&scc_stack, &member_to_process))
        {
            pending_count--;
        
            if(!discovery[member_to_process.index])
            {
                tarjan_dfs_visit(member_to_process);
            }
        }
        else
        {
            pending_count = 0; 
        }
    }
}