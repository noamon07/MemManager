#ifndef SCC_FINDER_H
#define SCC_FINDER_H

#include <stdint.h>
#include "Interface/mem_manager.h"
#include "Arenas/stack.h"
#include "Infrastructure/graph.h"


typedef struct { uint32_t disc; uint32_t low; } NodeTimes;
typedef struct 
{
    uint32_t max_nodes;
    uint32_t max_allowed_size;
    uint32_t size;
    Stack call_stack;
    Stack scc_stack;
    
    NodeTimes* node_times;

} Scc_Finder;
typedef struct {
    Handle Parent_handle;
    Edge edge_cursor;
} TarjanFrame;

Scc_Finder* scc_finder_init(uint32_t max_suspect_nodes, uint32_t max_allowed_size);

void scc_finder_destroy();

void recalculate_scc_subgraph(Handle start_handle);

void notify_edge_added(Handle node_A, Handle node_B);
void evaluate_scc_viability(Handle scc_representative);
int scc_finder_grow(uint32_t new_size);
#endif