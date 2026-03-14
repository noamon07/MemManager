#ifndef STRATEGY_TLSF_H
#define STRATEGY_TLSF_H
#define FLI_MAX        32
#include <cstdint>
#include <cstddef>
typedef struct
{
    char stratigy_id;
    void* heap_start;
    uint32_t flbitmap;//first level bitmap
    uint32_t sl_bitmap[FLI_MAX];/* Second-level bitmaps */
    void* free_list[FLI_MAX][32];// free list pointer for each size
} TLSFAllocator;
void tlsf_init(TLSFAllocator *t, size_t size);
void* tlsf_alloc(TLSFAllocator *t);
void tlsf_free(TLSFAllocator *t);
void tlsf_destroy(TLSFAllocator *t);
#endif