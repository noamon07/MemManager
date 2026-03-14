#ifndef MM_API_H
#define MM_API_H
#include <stddef.h>


typedef struct {
    unsigned int index;
    unsigned int generation;
} Handle;

// Optional: Manual init if the user wants to control startup
int mm_init();
void mm_destroy();
// Converts a Handle back to a pointer only when needed.
// This is where the Table check happens.
void* mm_get_ptr(Handle handle);


// The core API: Returns a Handle instead of a raw pointer
Handle mm_malloc(size_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, size_t new_size);
Handle mm_calloc(size_t size);


#endif