#ifndef BOUNDARY_BITMAP
#define BOUNDARY_BITMAP
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t is_allocated; 
    uint32_t is_start;     
} BitmapChunk;

typedef struct {
    BitmapChunk* chunks; 
} BoundaryBitmap;
// 1. Initialize the bitmaps at a specific memory address
void bitmap_init(BoundaryBitmap* bmp, void* memory_start);
// 2. Mark a new object when the Bump Allocator hands out memory
// This flips the first bit to 1 in both levels, and the rest to 1 only in Level 1
void bitmap_mark_allocation(BoundaryBitmap* bmp, size_t start_block, size_t block_count);

// 3. For the GC: Get the size of an object starting at a specific block
// The GC will use this to know exactly how many bytes to copy to Generation 1
size_t bitmap_get_object_size(BoundaryBitmap* bmp, size_t start_block);

// 4. Wipe the bitmaps clean (Call this after the GC moves the survivors)
void bitmap_clear_all(BoundaryBitmap* bmp);

#endif