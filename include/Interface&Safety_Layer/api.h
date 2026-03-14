#ifndef MM_API_H
#define MM_API_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "handle.h"

// Optional: Manual init if the user wants to control startup
int mm_init(void);

// The core API: Returns a Handle instead of a raw pointer
Handle mm_malloc(size_t size);
void mm_free(Handle handle);

// Converts a Handle back to a pointer only when needed.
// This is where the Table check happens.
void* mm_get_ptr(Handle handle);

// Metadata / Debugging
int mm_checkheap(int verbose);

#endif