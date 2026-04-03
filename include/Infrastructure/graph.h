#ifndef GRAPH_H
#define GRAPH_H

#include "Interface/mem_manager.h"
#include <stdint.h>
#ifndef INVALID_DATA_OFFSET
#define INVALID_DATA_OFFSET ((uint32_t)-1)
#endif

typedef struct {
    Handle child_handle; // The "Child" (where the pointer goes)
    uint32_t next_edge_offset;  // The next Edge in the linked list for the sam parent
} Edge;
void graph_init();
int graph_add_ref(Handle parent_handle, Handle child_handle);
int graph_remove_ref(Handle parent_handle, Handle child_handle);
void graph_clear_all_refs(Handle parent_handle);

#endif
