#include "InterfaceSafetyLayer/handle.h"

// We'll define our own sentinel to avoid magic numbers

void handle_table_init(HandleTable *table, void *initial_memory, size_t initial_size) {
    if (!table || !initial_memory || initial_size == 0) {
        return;
    }
    table->entries = (HandleEntry *)initial_memory;
    table->size = (uint32_t)initial_size;
    table->first_free_index = 0;

    // Initialize the free list manually
    for (uint32_t i = 0; i < table->size; ++i) {
        table->entries[i].generation = 0; 
        table->entries[i].is_allocated = 0;
        table->entries[i].stratigy_id = 0;
        
        // Link to the next entry
        if (i == table->size - 1) {
            table->entries[i].data.next_free_index = INVALID_INDEX;
        } else {
            table->entries[i].data.next_free_index = i + 1;
        }
    }
}

void handle_table_destroy(HandleTable *table) {
    if (table) {
        table->entries = NULL;
        table->size = 0;
        table->first_free_index = INVALID_INDEX;
    }
}

Handle handle_table_new(HandleTable *table, void *ptr, char stratigy_id) {
    Handle invalid_handle = {INVALID_INDEX, 0};
    if (!table || table->first_free_index == INVALID_INDEX || stratigy_id == 0) {
        return invalid_handle;
    }

    uint32_t index = table->first_free_index;
    HandleEntry *entry = &(table->entries[index]);

    // Pop from head of free list
    table->first_free_index = (uint32_t)entry->data.next_free_index;

    entry->is_allocated = 1;
    entry->data.ptr = ptr;
    entry->stratigy_id = stratigy_id;

    return (Handle){ index, entry->generation };
}

void *handle_table_get(HandleTable *table, Handle handle) {
    if (!table || handle.index >= table->size) {
        return NULL;
    }

    HandleEntry *entry = &(table->entries[handle.index]);

    if (!entry->is_allocated || entry->generation != handle.generation) {
        return NULL;
    }

    return entry->data.ptr;
}

void handle_table_free(HandleTable *table, Handle handle) {
    if (!table || handle.index >= table->size) {
        return;
    }

    HandleEntry *entry = &(table->entries[handle.index]);

    if (!entry->is_allocated || entry->generation != handle.generation) {
        return;
    }

    entry->is_allocated = 0;
    entry->generation++;

    // Push to head of free list
    entry->data.next_free_index = table->first_free_index;
    table->first_free_index = handle.index;
}

int handle_table_grow(HandleTable *table, void* new_memory, uint32_t added_size) {
    if (!table || !new_memory || added_size == 0 || added_size > (INVALID_INDEX - table->size)) {
        return -1;
    }
    HandleEntry *new_entries = (HandleEntry *)new_memory;
    uint32_t old_size = table->size;
    uint32_t new_size = old_size + added_size;

    for (uint32_t i = 0; i < old_size; ++i) {
        new_entries[i] = table->entries[i];
    }
    // Build the free list for the NEWLY added space
    for (uint32_t i = old_size; i < new_size; ++i) {
        new_entries[i].generation = 1;
        new_entries[i].is_allocated = 0;
        //new_entries
        
        if (i == new_size - 1) {
            // Last entry points to the OLD free list head
            new_entries[i].data.next_free_index = table->first_free_index;
        } else {
            new_entries[i].data.next_free_index = i + 1;
        }
    }

    table->entries = new_entries;
    table->size = new_size;
    table->first_free_index = old_size;

    return 0;
}