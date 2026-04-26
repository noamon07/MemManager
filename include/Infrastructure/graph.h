#ifndef GRAPH_H
#define GRAPH_H

#include "Interface/mem_manager.h"
#include <stdint.h>
#ifndef INVALID_DATA_OFFSET
#define INVALID_DATA_OFFSET ((uint32_t)-1)
#define INVALID_EDGE (Edge){INVALID_HANDLE, INVALID_DATA_OFFSET}
#endif

typedef struct {
    Handle child_handle; // The "Child" (where the pointer goes)
    uint32_t next_edge_offset;  // The next Edge in the linked list for the sam parent
} Edge;

int graph_init();
int graph_add_ref(Handle parent_handle, Handle child_handle);
int graph_remove_ref(Handle parent_handle, Handle child_handle);
void graph_free(Handle parent_handle);
Edge graph_get_first_edge(Handle handle);
Edge graph_get_edge(uint32_t offset);
Handle get_scc_root(Handle node);
int graph_in_same_scc(Handle handle_a, Handle handle_b);
#endif
