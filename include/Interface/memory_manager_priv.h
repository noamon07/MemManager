#ifndef MM_PRIV_H
#define MM_PRIV_H

#include <stddef.h>
void* mm_request_region(size_t size);
void* mm_resize_region(void* old_ptr, size_t old_size, size_t new_size);
#endif