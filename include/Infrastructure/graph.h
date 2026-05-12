#ifndef GRAPH_H
#define GRAPH_H

#include "Interface/mem_manager.h"
#include "Arenas/slab.h"
#include <stdint.h>

#ifndef INVALID_DATA_OFFSET
#define INVALID_DATA_OFFSET ((uint32_t)-1)
#endif

/** Sentinel value for an empty or null edge reference. */
#define INVALID_EDGE (Edge){INVALID_HANDLE, INVALID_DATA_OFFSET}

/**
 * @struct Edge
 * @brief Represents a directed pointer from one handle to another.
 */
typedef struct {
    Handle child_handle;        /* The target object being pointed to */
    uint32_t next_edge_offset;  /* Offset to the next Edge in the parent's adjacency list */
} Edge;

/* --- Lifecycle --- */

/**
 * @brief Initializes the global reference graph.
 * @param max_allowed_size Maximum memory allocated for the Edge slab.
 * @return Slab* Pointer to the internal slab used for edge storage.
 */
Slab* graph_init(uint32_t max_allowed_size);

/**
 * @brief Tears down the graph and frees the associated slab memory.
 */
void graph_destroy();

/* --- Edge Management --- */

/**
 * @brief Registers a reference from a parent object to a child object.
 * @return 1 on success, 0 on failure (e.g., slab full).
 */
int graph_add_ref(Handle parent_handle, Handle child_handle);

/**
 * @brief Removes a specific reference between two handles.
 * @return 1 if found and removed, 0 otherwise.
 */
int graph_remove_ref(Handle parent_handle, Handle child_handle);

/**
 * @brief Removes ALL outgoing references from a parent handle.
 * Typically called when an object is explicitly destroyed or collected.
 */
void graph_free(Handle parent_handle);

/* --- Graph Traversal --- */

/**
 * @brief Retrieves the start of the adjacency list for a given handle.
 */
Edge graph_get_first_edge(Handle handle);

/**
 * @brief Retrieves a specific edge by its slab offset.
 */
Edge graph_get_edge(uint32_t offset);

/* --- Advanced GC Analysis (SCC) --- */

/**
 * @brief Finds the root of the Strongly Connected Component for a node.
 * Part of the Tarjan's algorithm implementation.
 */
Handle get_scc_root(Handle node);

/**
 * @brief Checks if two objects are part of the same cyclic dependency.
 */
int graph_in_same_scc(Handle handle_a, Handle handle_b);

#endif /* GRAPH_H */