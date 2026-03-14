#include "strategies/bump.h"
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

int bump_init(BumpAllocator *a, size_t data_size) {
    /* 1. Calculate bitmap requirements */
    size_t num_units = data_size / ALIGNMENT;
    size_t bmap_bytes = (num_units + 7) / 8;
    
    /* 2. Total memory needed: 2 bitmaps + the heap itself */
    size_t total_needed = (bmap_bytes * 2) + data_size;
    
    /* 3. Get raw memory from the OS */
    void *mem = mmap(NULL, total_needed, PROT_READ | PROT_WRITE, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (mem == MAP_FAILED) return 0;

    /* 4. Carve up the block */
    a->base_ptr = (unsigned char*)mem;
    a->live_bits = a->base_ptr;
    a->size_bits = a->base_ptr + bmap_bytes;
    a->heap_start = a->base_ptr + (bmap_bytes * 2);
    
    /* Ensure the heap start is still aligned after the bitmaps */
    /* If bmap_bytes*2 isn't a multiple of 8, we might need a tiny padding */
    
    a->next_alloc = a->heap_start;
    a->capacity = data_size;
    a->bitmap_size = bmap_bytes;
    a->total_mmap_size = total_needed;

    return 1;
}
void bump_reset(BumpAllocator *a)
{
    (void)a;
}

void* bump_alloc(BumpAllocator *a, size_t size) {
    size_t aligned_size = ALIGN(size);
    if (a->next_alloc + aligned_size > a->heap_start + a->capacity) {
        return NULL;
    }
    
    void *ptr = a->next_alloc;
    a->next_alloc += aligned_size;
    return ptr;
}

void bump_mark_alive(BumpAllocator *a, void *ptr, size_t size) {
    size_t offset = (unsigned char*)ptr - a->heap_start;
    size_t unit_idx = offset / ALIGNMENT;
    size_t num_units = ALIGN(size) / ALIGNMENT;

    a->live_bits[unit_idx / 8] |= (1 << (unit_idx % 8));
    
    size_t end_unit = unit_idx + num_units - 1;
    a->size_bits[end_unit / 8] |= (1 << (end_unit % 8));
}

void* bump_move_live(BumpAllocator *a, void *dest_heap) {
    unsigned char *dest = (unsigned char*)dest_heap;
    /* num_units is based on how far we actually allocated, not total capacity */
    size_t used_units = (a->next_alloc - a->heap_start) / ALIGNMENT;
    size_t i;

    for (i = 0; i < used_units; i++) {
        if (a->live_bits[i / 8] & (1 << (i % 8))) {
            size_t start_unit = i;
            /* Scan for the end bit */
            while (i < used_units && !(a->size_bits[i / 8] & (1 << (i % 8)))) {
                i++;
            }
            
            size_t object_size = (i - start_unit + 1) * ALIGNMENT;
            memcpy(dest, a->heap_start + (start_unit * ALIGNMENT), object_size);
            dest += object_size;
        }
    }
    
    /* Important: Reset your own bitmaps for the next cycle after moving */
    memset(a->live_bits, 0, a->bitmap_size * 2);
    a->next_alloc = a->heap_start; 
    
    return dest;
}

void bump_destroy(BumpAllocator *a) {
    if (a->base_ptr) {
        munmap(a->base_ptr, a->total_mmap_size);
    }
}