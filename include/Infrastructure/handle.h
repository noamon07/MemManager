#ifndef HANDLE_H
#define HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include "Interface/mem_manager.h"
#include "Strategies/strategy.h"

/**
 * @struct HandleEntry
 * @brief Represents a single slot in the Handle Table.
 */
typedef struct HandleEntry {
    Strategy* strategy;     /* Pointer to the allocation strategy (Bump/TLSF) */
    union {
        uint32_t data_offset; /* Physical offset in the arena buffer */
        uint32_t next;        /* Free-list link when is_allocated is 0 */
    } data;
    
    uint32_t size;            /* Logical size of the allocated object */
    
    union {
        Handle root_scc;      /* Root of the Strongly Connected Component */
        uint32_t external_in_degree; /* Used for cycle-aware collection */
    } scc;
    
    Handle next_in_scc;       /* Linked list of nodes within the same SCC */
    uint32_t first_edge_offset; /* Offset to the first Edge in the Graph Slab */
    uint16_t in_degree;       /* Reference count (total incoming edges) */
    uint8_t generation;       /* Counter used to detect stale handles */
    
    /* GC and State Flags */
    uint8_t is_allocated:1,     /* Slot is currently in use */
            is_scc_suspect:1,   /* Flagged for potential cycle detection */
            is_scc_root:1,      /* Node is the entry point of an SCC */
            is_on_scc_stack:1,  /* Internal Tarjan's algorithm state */
            is_root:1;          /* Explicitly pinned (pinned by user) */
} HandleEntry;

/**
 * @struct HandleTable
 * @brief The global registry for all active memory handles.
 */
typedef struct {
    HandleEntry *entries;      /* Dynamic array of entries */
    uint32_t size;             /* Current number of slots in the table */
    uint32_t head;             /* Start of the free-list for slot reuse */
    uint32_t max_allowed_size; /* Ceiling for table expansion */
} HandleTable;

/* --- Lifecycle --- */

/**
 * @brief Initializes the global handle table.
 */
HandleTable* handle_table_init(uint32_t initial_capacity, uint32_t max_allowed_size);

/**
 * @brief Tears down the table and frees entry memory.
 */
void handle_table_destroy();

/* --- Entry Management --- */

/**
 * @brief Reserves a new slot and returns a Handle.
 * @param ptr_size Initial size request for the object.
 * @return A Handle (packed index and generation).
 */
Handle handle_table_new(uint32_t ptr_size);

/**
 * @brief Resolves a Handle to a raw memory pointer.
 * Note: Pointers returned here are volatile if defragmentation occurs.
 */
void *handle_table_get_ptr(Handle handle);

/**
 * @brief Looks up a HandleEntry by its raw index.
 */
HandleEntry *handle_table_get_entry_by_index(uint32_t index);

/**
 * @brief Looks up a HandleEntry by its Handle (verifies generation).
 */
HandleEntry *handle_table_get_entry(Handle handle);

/**
 * @brief Returns a slot to the free-list and increments its generation.
 */
void handle_table_free(Handle handle);

/* --- Maintenance --- */

/**
 * @brief Resizes the handle table array if capacity is reached.
 */
int handle_table_grow();

/**
 * @brief Singleton accessor for the system's Handle Table.
 */
HandleTable *mm_get_handle_table_instance();

#endif /* HANDLE_H */