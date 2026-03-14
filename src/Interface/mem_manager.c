#include "Interface/mem_manager.h"
#include "strategies/slab.h"   /* Your Slab Allocator internal header */
#include "Interface/handle.h" /* Your Handle Table internal header */

/* Static globals keep these hidden from the user (Encapsulation) */
static int initialized = 0;
static HandleTable table;

int mm_init() {
    //SlabAllocator slab;
    if (initialized) return 0;

    /* 1. Initialize your Slab/Buddy system here */
    //slab_init(&slab,sizeof(HandleEntry));
    /* 2. Initialize the Handle Table the number of entries is the size of the slab allocator 1 page/size of an entry*/
    //handle_table_init(&table,slab.heap_start, 4096/sizeof(HandleEntry));
    
    initialized = 1;
    return 1;
}

void mm_destroy()
{
    
}

Handle mm_malloc(size_t size) {
    (void)size;
    Handle h = {INVALID_INDEX, 0};

    /* Lazy initialization check */
    if (!initialized) {
        if (!mm_init()) return h;
    }

    /* 1. Allocate raw memory from your Slab Allocator */
    //void* raw_ptr = slab_alloc(size);
    //if (!raw_ptr) return h;

    /* 2. Register that pointer in the Handle Table to get a Handle */
    //h = handle_table_new(&table, raw_ptr,"S");

    return h;
}

void* mm_get_ptr(Handle handle) {
    if (!initialized) return NULL;

    /* Direct lookup in the table */
    return handle_table_get(&table, handle);
}

void mm_free(Handle handle) {
    if (!initialized) return;

    /* 1. Find the pointer to free it from the Slab */
    void* ptr = handle_table_get(&table, handle);
    if (ptr) {
        //slab_free(ptr);
    }

    /* 2. Invalidate the handle in the table */
    handle_table_free(&table, handle);
}