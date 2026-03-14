#ifndef BUMP_H
#define BUMP_H

#include <stddef.h>

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

typedef struct {
    unsigned char *base_ptr;    /* Start of the entire mmap'd region */
    unsigned char *heap_start;  /* Start of actual data area */
    unsigned char *next_alloc;
    size_t capacity;            /* Data capacity only */
    
    unsigned char *live_bits;
    unsigned char *size_bits;
    size_t bitmap_size;
    size_t total_mmap_size;     /* Total size including bitmaps */
} BumpAllocator;

int bump_init(BumpAllocator *a, size_t data_size);
void* bump_alloc(BumpAllocator *a, size_t size);
void  bump_mark_alive(BumpAllocator *a, void *ptr, size_t size);
void* bump_move_live(BumpAllocator *a, void *dest_heap);
void  bump_destroy(BumpAllocator *a);

#endif