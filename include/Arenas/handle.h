#ifndef HANDLE_H
#define HANDLE_H

#include <stddef.h>
#include <stdint.h>
#include "Interface/mem_manager.h"

typedef enum {
    ALLOC_TYPE_ERROR,
    ALLOC_TYPE_NURSERY,
    ALLOC_TYPE_TLSF,
    ALLOC_TYPE_SLAB,
    ALLOC_TYPE_HUGE,
    ALLOC_TYPE_MAX,
}alloc_type_t;

typedef struct HandleEntry HandleEntry;

struct HandleEntry {
    union {
        void *ptr; // Points to the user's data
        uint32_t next; // Index of the next free entry in the list
    } data;
    uint32_t generation;
    alloc_type_t stratigy_id;
    uint32_t size;
    // A flag to know if the entry is in use or not.
    // This simplifies logic compared to checking generation == 0.
    char is_allocated;
};

typedef struct {
    HandleEntry *entries;
    // capacity can be uint32_t because the size of an entry is 16 byte and the maximum number in uint32_t is 4bilion
    //the maximum amount is about 68 Gigabytes of RAM withount counting the data only the handles so you will run out of ram way before you get to it
    uint32_t size;
    uint32_t head; // Head of the free list
} HandleTable;

void handle_table_init(HandleTable *table, uint32_t initial_capacity);
void handle_table_destroy(HandleTable *table);
Handle handle_table_new(HandleTable *table, void *ptr, uint32_t ptr_size, alloc_type_t stratigy_id);
void *handle_table_get_ptr(HandleTable *table, Handle handle);
HandleEntry *handle_table_get_entry(HandleTable *table, Handle handle);
void handle_table_free(HandleTable *table, Handle handle);
int handle_table_grow(HandleTable *table, uint32_t added_size);

#endif // HAND