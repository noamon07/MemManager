#ifndef GENERAL_ALLOCATOR_H
#define GENERAL_ALLOCATOR_H

#include <stdint.h>
#include "Arenas/handle.h"

#define TLSF_FL_INDEX_MAX 32 //128 MB
#define TLSF_SL_INDEX_COUNT 16
typedef struct General {
    uint8_t* mem;                  /* Pointer to the start of the managed memory pool */
    uint32_t mem_size;             /* Total size of the memory pool */
    uint32_t alloc_memory;         /* Currently allocated memory in bytes */
    
    /* TLSF-specific control structures */
    uint32_t fl_bitmap;                                         /* First-Level bitmap */
    uint32_t sl_bitmap[TLSF_FL_INDEX_MAX];                      /* Second-Level bitmaps */
    uint32_t blocks[TLSF_FL_INDEX_MAX][TLSF_SL_INDEX_COUNT];  /* Heads of free block lists */
} General;


#define INVALID_GENERAL_INDEX ((uint32_t)-1)
void mm_general_destroy();
uint32_t mm_malloc_general(uint32_t size, uint32_t** entry_index);
void mm_free_general(data_pos data);
uint32_t mm_realloc_general(data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index);
uint32_t mm_calloc_general(uint32_t size, uint32_t** entry_index);
void* mm_general_get(HandleEntry* entry);
General* mm_get_general_instance(void);
#endif // GENERAL_ALLOCATOR_H