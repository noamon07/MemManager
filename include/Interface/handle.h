#ifndef HANDLE_H
#define HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include "Interface/mem_manager.h"
#define INVALID_INDEX ((uint32_t)-1)


// Using a union to make the free list implementation cleaner.
typedef struct {
    union {
        void *ptr; // Points to the user's data
        size_t next_free_index; // Index of the next free entry in the list
    } data;
    unsigned int generation;
    char stratigy_id;
    // A flag to know if the entry is in use or not.
    // This simplifies logic compared to checking generation == 0.
    unsigned char is_allocated;
} HandleEntry;

typedef struct {
    HandleEntry *entries;
    // capacity can be uint32_t because the size of an entry is 16 byte and the maximum number in uint32_t is 4bilion
    //the maximum amount is about 68 Gigabytes of RAM withount counting the data only the handles so you will run out of ram way before you get to it
    uint32_t size;
    uint32_t first_free_index; // Head of the free list
} HandleTable;

void handle_table_init(HandleTable *table, void *initial_memory, size_t initial_capacity);
void handle_table_destroy(HandleTable *table);
Handle handle_table_new(HandleTable *table, void *ptr, char stratigy_id);
void *handle_table_get(HandleTable *table, Handle handle);
void handle_table_free(HandleTable *table, Handle handle);
int handle_table_grow(HandleTable *table, void* new_memory, uint32_t added_size);

#endif // HAND