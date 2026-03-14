#ifndef MM_API_H
#define MM_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "handle.h"

// Optional: Manual init if the user wants to control startup
int mm_init();
void mm_destroy();
// The core API: Returns a Handle instead of a raw pointer
Handle mm_malloc(size_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, size_t new_size);
Handle mm_calloc(size_t size);

// Converts a Handle back to a pointer only when needed.
// This is where the Table check happens.
void* mm_get_ptr(Handle handle);
#endif