#ifndef MM_API_H
#define MM_API_H
#include <stdint.h>

typedef struct {
    uint32_t index;
    uint32_t generation;
} Handle;

#define INVALID_INDEX ((uint32_t)-1)

// Optional: Manual init if the user wants to control startup
void mm_destroy();
// Converts a Handle back to a pointer only when needed.
// This is where the Table check happens.
void* mm_get_ptr(Handle handle);


// The core API: Returns a Handle instead of a raw pointer
Handle mm_malloc(uint32_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, uint32_t new_size);
Handle mm_calloc(uint32_t size);


#endif