#include <stddef.h>

void* mm_malloc(size_t size);
void mm_free(void* ptr);
void* mm_realloc(void* ptr, size_t size);
int mm_checkheap(int verbose);