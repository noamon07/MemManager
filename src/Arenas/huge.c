#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/mman.h> 

#include "Arenas/huge.h"

void* mm_malloc_huge(uint32_t size)
{
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return NULL;
    return ptr;
}
void mm_free_huge(data_pos data, uint32_t size)
{
    munmap(data.ptr, size);
}
void* mm_realloc_huge(data_pos data,uint32_t ptr_size, uint32_t new_size)
{
    void* new_ptr = mremap(data.ptr,ptr_size, new_size,PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_ptr == MAP_FAILED) return NULL;
    return new_ptr;

}
void* mm_calloc_huge(uint32_t size)
{
    return mm_malloc_huge(size);
}
void* mm_huge_get(HandleEntry* entry)
{
    if(entry->stratigy_id != ALLOC_TYPE_HUGE)
        return NULL;
    return entry->data.data_ptr.ptr;
}
