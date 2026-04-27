#ifndef HANDLE_H
#define HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include "Interface/mem_manager.h"
#include "Strategies/strategy.h"


typedef struct HandleEntry {
    Strategy* strategy;
    union {
        uint32_t data_offset;
        uint32_t next;
    } data;
    uint32_t size;
    union 
    {
        Handle root_scc;
        uint32_t external_in_degree;
    }scc;
    Handle next_in_scc;
    uint32_t first_edge_offset;
    uint16_t in_degree;
    uint8_t generation;
    uint8_t is_allocated:1,
            is_scc_suspect:1,
            is_scc_root:1,
            is_on_scc_stack:1,
            is_root:1;
} HandleEntry;

typedef struct {
    HandleEntry *entries;
    uint32_t size;
    uint32_t head;
    uint32_t max_allowed_size;
} HandleTable;

HandleTable* handle_table_init(uint32_t initial_capacity, uint32_t max_allowed_size);
void handle_table_destroy();
Handle handle_table_new(uint32_t ptr_size);
void *handle_table_get_ptr(Handle handle);
HandleEntry *handle_table_get_entry_by_index(uint32_t index);
HandleEntry *handle_table_get_entry(Handle handle);
void handle_table_free(Handle handle);
int handle_table_grow();
HandleTable *mm_get_handle_table_instance();

#endif