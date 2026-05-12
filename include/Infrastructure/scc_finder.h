#ifndef SCC_FINDER_H
#define SCC_FINDER_H

#include <stdint.h>
#include "Interface/mem_manager.h"
#include "Arenas/stack.h"
#include "Infrastructure/graph.h"

/**
 * @struct NodeTimes
 * @brief Tracking metadata for Tarjan's discovery phase.
 */
typedef struct { 
    uint32_t disc; /* Discovery time: order in which the node was visited */
    uint32_t low;  /* Low-link value: smallest discovery time reachable in DFS */
} NodeTimes;

/**
 * @struct Scc_Finder
 * @brief State container for the SCC analysis engine.
 */
typedef struct 
{
    uint32_t max_nodes;         /* Maximum nodes supported by the current buffers */
    uint32_t max_allowed_size;  /* Ceiling for memory expansion */
    uint32_t size;              /* Current number of monitored nodes */
    
    Stack call_stack;           /* Simulates recursion for the DFS traversal */
    Stack scc_stack;            /* Tracks nodes in the current SCC candidate */
    
    NodeTimes* node_times;      /* Array of discovery metadata indexed by Handle */
} Scc_Finder;

/**
 * @struct TarjanFrame
 * @brief An activation record for the non-recursive DFS implementation.
 */
typedef struct {
    Handle Parent_handle;       /* The node currently being explored */
    Edge edge_cursor;           /* Current position in the node's adjacency list */
} TarjanFrame;

/* --- Lifecycle --- */

/**
 * @brief Initializes the SCC Finder engine.
 * Pre-allocates discovery buffers and internal stacks.
 */
Scc_Finder* scc_finder_init(uint32_t max_suspect_nodes, uint32_t max_allowed_size);

/**
 * @brief Tears down the SCC engine and releases buffer memory.
 */
void scc_finder_destroy();

/* --- Core Analysis --- */

/**
 * @brief Executes Tarjan's algorithm on a specific subgraph.
 * 
 * Triggered when a reference is removed, this identifies if the 
 * remaining objects still form a valid cycle or should be collected.
 * 
 * @param start_handle The entry point for the subgraph analysis.
 */
void recalculate_scc_subgraph(Handle start_handle);

/**
 * @brief Hook for the graph manager to alert the engine of new references.
 */
void notify_edge_added(Handle node_A, Handle node_B);

/**
 * @brief Determines if an SCC is still reachable from the global roots.
 * If not, the entire component is flagged for deletion.
 */
void evaluate_scc_viability(Handle scc_representative);

/* --- Maintenance --- */

/**
 * @brief Expands the discovery and metadata buffers.
 */
int scc_finder_grow(uint32_t new_size);

#endif /* SCC_FINDER_H */