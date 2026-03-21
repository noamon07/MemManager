#include "Arenas/handle.h"
// #include "Arenas/nursery.h"
// #include "Arenas/huge.h"
#include <stdlib.h>
#include <string.h>
#include "Strategies/nursery.h"

// We'll define our own sentinel to avoid magic numbers
static HandleTable table;
static char initialized = 0;

void handle_table_init(uint32_t initial_size) {
    if (initialized||initial_size == 0) {
        return;
    }
    table.entries = (HandleEntry*)calloc(initial_size * sizeof(HandleEntry),1);
    table.size = (uint32_t)initial_size;
    table.head = 0;

    // Initialize the free list manually
    table.entries[initial_size-1].data.next = INVALID_INDEX;
    for (uint32_t i = 0; i < table.size-1; ++i) {
        // Link to the next entry
        table.entries[i].data.next = i + 1;
    }
    initialized = 1;
}

void handle_table_destroy() {
    if (initialized) {
        if(table.entries)
        {
            free(table.entries);
        }
        table.entries = NULL;
        table.size = 0;
        table.head = 0;
        initialized = 0;
    }
}

Handle handle_table_new(uint32_t ptr_size, alloc_type_t stratigy_id){
    Handle invalid_handle = {INVALID_INDEX, 0};
    if (!initialized || stratigy_id <= ALLOC_TYPE_ERROR || stratigy_id >= ALLOC_TYPE_MAX){
        return invalid_handle;
    }
    if(table.head == INVALID_INDEX)
    {
        if(!handle_table_grow())
            return invalid_handle;
    }
    HandleEntry *entry = &table.entries[table.head];
    uint32_t index = table.head;

    // Pop from head of free list
    table.head = entry->data.next;
    entry->size = ptr_size;
    entry->is_allocated = 1;
    entry->stratigy_id = stratigy_id;

    return (Handle){ index, entry->generation };
}

void *handle_table_get_ptr(Handle handle) {
    HandleEntry* entry = handle_table_get_entry(handle);
    if (!entry) {
        return NULL;
    }
    switch (entry->stratigy_id)
    {
        case ALLOC_TYPE_NURSERY:
            return nursery_get(entry);
            break;
        case ALLOC_TYPE_GENERAL:
            break;
        case ALLOC_TYPE_SLAB:
            break;
        case ALLOC_TYPE_HUGE:
            // return mm_huge_get(entry);
            break;
        default:
            break;
    }
    return NULL;
}

HandleEntry *handle_table_get_entry_by_index(uint32_t index)
{
    if (!initialized || index >= table.size) {
        return NULL;
    }

    HandleEntry *entry = &(table.entries[index]);

    if (!entry->is_allocated) {
        return NULL;
    }

    return entry;
}

HandleEntry *handle_table_get_entry(Handle handle)
{
    HandleEntry *entry = handle_table_get_entry_by_index(handle.index);
    if (!entry) {
        return NULL;
    }
    if(entry->generation != handle.generation)
    {
        return NULL;
    }

    return entry;
}



void handle_table_free(Handle handle) {
    HandleEntry *entry = handle_table_get_entry(handle);
    if (!entry) {
        return;
    }
    entry->is_allocated = 0;
    entry->generation++;

    // Push to head of free list
    entry->data.next = table.head;
    table.head = handle.index;
}

int handle_table_grow() {
    if (!initialized || table.size > (INVALID_INDEX - table.size)) {
        return 0;
    }
    HandleEntry *new_entries = (HandleEntry *)realloc(table.entries, (table.size + table.size) * sizeof(HandleEntry));
    if(!new_entries)
        return 0;
    memset(&new_entries[table.size], 0, table.size * sizeof(HandleEntry));
    uint32_t old_size = table.size;
    uint32_t new_size = old_size + table.size;
    // Build the free list for the NEWLY added space
    new_entries[new_size-1].data.next = table.head;
    for (uint32_t i = old_size; i < new_size-1; ++i) {
        new_entries[i].data.next = i + 1;
    }

    table.entries = new_entries;
    table.size = new_size;
    table.head = old_size;
    return 1;
}
HandleTable *mm_get_handle_table_instance()
{
    if(!initialized)
        return NULL;
    return &table;
}