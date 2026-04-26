#include "Infrastructure/scc_finder.h"
#include "Infrastructure/graph.h"
#include "Infrastructure/handle.h"
#include "Interface/mem_manager.h"
#include <stdlib.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif



static Scc_Finder scc_finder;
static uint32_t current_time = 0;

Scc_Finder* scc_finder_init(uint32_t max_suspect_nodes, uint32_t max_allowed_size)
{    
    int fail = !stack_init(&scc_finder.call_stack, max_suspect_nodes, sizeof(TarjanFrame));
    fail |= !stack_init(&scc_finder.scc_stack, max_suspect_nodes, sizeof(Handle));
    scc_finder.node_times = (NodeTimes*)calloc(max_suspect_nodes, sizeof(NodeTimes));
    fail |= !scc_finder.node_times;
    scc_finder.max_nodes = max_suspect_nodes;
    scc_finder.max_allowed_size = max_allowed_size;
    scc_finder.size = max_suspect_nodes * sizeof(TarjanFrame) + max_suspect_nodes * sizeof(Handle) + max_suspect_nodes * sizeof(NodeTimes);
    if(fail)
    {
        scc_finder_destroy();
        return NULL;
    }
    return &scc_finder;
}

void scc_finder_destroy()
{
    stack_destroy(&scc_finder.call_stack);
    stack_destroy(&scc_finder.scc_stack);
    free(scc_finder.node_times);
}

void evaluate_scc_viability(Handle scc_representative)
{
    HandleEntry* root_entry = handle_table_get_entry(scc_representative);
    if(!root_entry) return;

    uint32_t total_in_degree = 0;
    uint32_t internal_edges = 0;
    Handle current_node = scc_representative;
    while(is_valid_handle(current_node))
    {
        HandleEntry* current_entry = handle_table_get_entry(current_node);
        if(current_entry)
        {
            total_in_degree += current_entry->in_degree;
            Edge current_edge = graph_get_first_edge(current_node);
            
            while(is_valid_handle(current_edge.child_handle)) 
            {
                HandleEntry* child_entry = handle_table_get_entry(current_edge.child_handle);
                if (child_entry && 
                   ((child_entry->is_scc_root && handles_equal(current_edge.child_handle,scc_representative)) ||
                   (!child_entry->is_scc_root && handles_equal(child_entry->scc.root_scc,scc_representative))))
                {
                    internal_edges++;
                }
                current_edge = graph_get_edge(current_edge.next_edge_offset);
            }
            current_node = current_entry->next_in_scc;
        }
        else
        {
            current_node = INVALID_HANDLE;
        }
    }

    root_entry->scc.external_in_degree = total_in_degree - internal_edges;

    if (root_entry->scc.external_in_degree == 0) 
    {
        Handle dead_node = scc_representative;
        
        while (is_valid_handle(dead_node)) 
        {
            HandleEntry* dead_entry = handle_table_get_entry(dead_node);
            if (dead_entry)
            {
                Handle next_dead = dead_entry->next_in_scc;
                
                mm_free(dead_node);
                dead_node = next_dead;
            }
            else
            {
                dead_node = INVALID_HANDLE;
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
    
    while(!is_root_found && stack_pop(&scc_finder.scc_stack, &current_node))
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

            if((scc_finder.node_times +current_node.index)->disc == (scc_finder.node_times +current_node.index)->low)
            {
                entry->next_in_scc = INVALID_HANDLE;
                is_root_found = 1;
            }
            else
            {
                Handle ph;
                peeked_handle = &ph;
                if(stack_top(&scc_finder.scc_stack, (void**)&peeked_handle))
                {
                    entry->next_in_scc = *peeked_handle;
                }
                else
                {
                    entry->next_in_scc = INVALID_HANDLE;
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
    (scc_finder.node_times +start_node.index)->disc = ++current_time;
    
    (scc_finder.node_times +start_node.index)->low = current_time;
    
    Edge first_edge = graph_get_first_edge(start_node);
    TarjanFrame initial_frame = {start_node, first_edge};
    
    stack_push(&scc_finder.call_stack, &initial_frame);
    stack_push(&scc_finder.scc_stack, &start_node);
    entry->is_on_scc_stack = 1;

    int error_flag = 0;

    while(scc_finder.call_stack.count != 0 && !error_flag)
    {
        TarjanFrame* frame = NULL;
        if (!stack_top(&scc_finder.call_stack, (void**)&frame) || frame == NULL) 
        {
            error_flag = 1; 
        }
        else
        {
            Handle current_node = frame->Parent_handle;
            Edge current_edge = frame->edge_cursor;

            if(is_valid_handle(current_edge.child_handle))
            {
                frame->edge_cursor = graph_get_edge(current_edge.next_edge_offset);
                
                Handle child_node = current_edge.child_handle;
                HandleEntry* child_entry = handle_table_get_entry(child_node);
                
                if(!(scc_finder.node_times +child_node.index)->disc)
                {
                    (scc_finder.node_times +child_node.index)->disc = ++current_time;
                    (scc_finder.node_times +child_node.index)->low = current_time;
                    Edge child_edge = graph_get_first_edge(child_node);
                    
                    TarjanFrame child_frame = {child_node, child_edge};
                    stack_push(&scc_finder.call_stack, &child_frame);
                    stack_push(&scc_finder.scc_stack, &child_node);
                    
                    if (child_entry) child_entry->is_on_scc_stack = 1;
                }
                else if(child_entry && child_entry->is_on_scc_stack)
                {
                    (scc_finder.node_times +child_node.index)->low = MIN((scc_finder.node_times +child_node.index)->low , (scc_finder.node_times +child_node.index)->disc);
                }
            }
            else
            {
                TarjanFrame dummy; 
                stack_pop(&scc_finder.call_stack, &dummy);
                
                TarjanFrame* parent_frame = NULL;
                if(stack_top(&scc_finder.call_stack, (void**)&parent_frame))
                {
                    Handle parent_node = parent_frame->Parent_handle;
                    (scc_finder.node_times +parent_node.index)->low = MIN((scc_finder.node_times +parent_node.index)->low, (scc_finder.node_times +current_node.index)->low);
                }
                
                if((scc_finder.node_times +current_node.index)->disc == (scc_finder.node_times +current_node.index)->low)
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

    if(!entry->is_scc_root && is_valid_handle(entry->scc.root_scc))
    {
        start_handle = entry->scc.root_scc;
        entry = handle_table_get_entry(start_handle);
        if (!entry) return;
    }
    
    Handle current_old = start_handle;
    uint32_t pending_count = 0;
    
    while(is_valid_handle(current_old))
    {
        HandleEntry* old_entry = handle_table_get_entry(current_old);
        if (old_entry)
        {
            Handle next_old = old_entry->next_in_scc;
            
            old_entry->is_scc_root = 0;
            old_entry->next_in_scc = INVALID_HANDLE;
            old_entry->scc.root_scc = INVALID_HANDLE;
            (scc_finder.node_times +current_old.index)->disc = 0;
            
            stack_push(&scc_finder.scc_stack, &current_old);
            pending_count++;
            
            current_old = next_old;
        }
        else
        {
            current_old = INVALID_HANDLE;
        }
    }
        
    while(pending_count > 0)
    {
        Handle member_to_process;
        if (stack_pop(&scc_finder.scc_stack, &member_to_process))
        {
            pending_count--;
            if(!(scc_finder.node_times +member_to_process.index)->disc)
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
int lightweight_bfs_search(Handle start_node, Handle target_node, Handle* out_path)
{
    uint32_t* memory_pool = (uint32_t*)scc_finder.call_stack.data;
    Handle* bfs_queue   = (Handle*)memory_pool; 
    Handle* bfs_parent  = (Handle*)(memory_pool + scc_finder.max_nodes);
    uint32_t* bfs_visited = memory_pool + (2 * scc_finder.max_nodes);
    
    memset(bfs_visited, 0, scc_finder.max_nodes * sizeof(uint32_t));
    uint32_t head = 0;
    uint32_t tail = 0;

    bfs_queue[tail++] = start_node;
    bfs_visited[start_node.index] = 1;
    bfs_parent[start_node.index] = INVALID_HANDLE;

    int cycle_found = 0;

    while (head < tail && !cycle_found)
    {
        Handle current_node = bfs_queue[head++]; 
        
        if (handles_equal(current_node, target_node)) {
            cycle_found = 1;
        }
        else
        {
            if (is_valid_handle(current_node))
            {
                Edge e = graph_get_first_edge(current_node);
                while (is_valid_handle(e.child_handle))
                {
                    if (bfs_visited[e.child_handle.index] == 0)
                    {
                        bfs_visited[e.child_handle.index] = 1;
                        bfs_parent[e.child_handle.index] = current_node;
                        bfs_queue[tail++] = e.child_handle;
                    }
                    e = graph_get_edge(e.next_edge_offset);
                }
            }
        }
    }

    if (!cycle_found) return 0;

    uint32_t path_length = 0;
    Handle path_tracer = target_node;
    while (is_valid_handle(path_tracer))
    {
        out_path[path_length++] = path_tracer;
        path_tracer = bfs_parent[path_tracer.index];
    }
    
    return path_length;
}
void notify_edge_added(Handle node_A, Handle node_B)
{
    Handle master_root = get_scc_root(node_B);
    Handle target_root = get_scc_root(node_A);
    
    if (!is_valid_handle(master_root) || !is_valid_handle(target_root)) return;
    if (handles_equal(master_root, target_root)) return;

    Handle* exact_cycle_path = (Handle*)scc_finder.scc_stack.data; 
    
    int path_len = lightweight_bfs_search(node_B, node_A, exact_cycle_path);
    if (path_len == 0)
    {
        HandleEntry* master_entry = handle_table_get_entry(master_root);
        master_entry->scc.external_in_degree++;
        return;
    }

    HandleEntry* master_entry = handle_table_get_entry(master_root);

    for (int i = 0; i < path_len; i++)
    {
        Handle node_on_path = exact_cycle_path[i];
        Handle local_root = get_scc_root(node_on_path);
        
        if (!handles_equal(local_root, master_root))
        {
            Handle current_member = local_root;
            Handle tail = INVALID_HANDLE;
        
            while (is_valid_handle(current_member))
            {
                HandleEntry* member_entry = handle_table_get_entry(current_member);
                if (!member_entry) break;

                member_entry->is_scc_root = 0;
                member_entry->scc.root_scc = master_root;
                
                tail = current_member;
                current_member = member_entry->next_in_scc;
            }

            if (is_valid_handle(tail))
            {
                HandleEntry* tail_entry = handle_table_get_entry(tail);
                tail_entry->next_in_scc = master_entry->next_in_scc;
                master_entry->next_in_scc = local_root;
            }
        }
    }
    evaluate_scc_viability(master_root);
}
int scc_finder_grow(uint32_t max_suspect_nodes)
{
    stack_destroy(&scc_finder.call_stack);
    stack_destroy(&scc_finder.scc_stack);
    int fail = !stack_init(&scc_finder.call_stack, max_suspect_nodes, sizeof(TarjanFrame));
    fail |= !stack_init(&scc_finder.scc_stack, max_suspect_nodes, sizeof(Handle));
    free(scc_finder.node_times);
    scc_finder.node_times = (NodeTimes*)calloc(scc_finder.max_nodes, sizeof(NodeTimes));
    fail |= !scc_finder.node_times;
    scc_finder.max_nodes = max_suspect_nodes;
    if(fail)
    {
        scc_finder_destroy();
        return 0;
    }
    return 1;
}