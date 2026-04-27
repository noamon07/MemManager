#ifndef MM_PRIV_H
#define MM_PRIV_H

#include <stddef.h>
void* mm_request_region(size_t size);
void* mm_resize_region(void* old_ptr, uint32_t old_size, uint32_t new_size, uint32_t max_allowed_size);
#endif