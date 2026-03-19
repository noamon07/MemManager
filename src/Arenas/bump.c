#include "Arenas/bump.h"
#include <stdlib.h>
#include <string.h>

int bump_init(BumpAllocator* bump, uint32_t initial_size, bump_defrag_cb cb, void* context) {
    if (!bump || initial_size == 0) return 0;

    bump->mem = (uint8_t*)calloc(initial_size, 1);
    if (!bump->mem) return 0;

    bump->mem_size = initial_size;
    bump->cur_index = 0;
    bump->alloc_memory = 0;
    bump->defrag_cb = cb;
    bump->arena_context = context;

    return 1;
}

void bump_destroy(BumpAllocator* bump) {
    if (!bump || !bump->mem) return;
    
    free(bump->mem);
    bump->mem = NULL;
    bump->mem_size = 0;
    bump->cur_index = 0;
    bump->alloc_memory = 0;
    bump->defrag_cb = NULL;
    bump->arena_context = NULL;
}

uint32_t bump_malloc(BumpAllocator* bump, uint32_t size, Handle handle, uint32_t custom_flags) {
    if (!bump ||!bump->mem) return INVALID_DATA_OFFSET;

    if(size<sizeof(BaseFooter))
        size = sizeof(BaseFooter);
    size+=sizeof(BaseHeader);
    size = ALIGN(size);

    /* Guard: Check if we are out of bounds */
    if (bump->cur_index + size > bump->mem_size) {
        return INVALID_DATA_OFFSET; 
    }

    BaseHeader* header = (BaseHeader*)(bump->mem + bump->cur_index);
    
    header->size = BYTES_TO_HEADER_SIZE(size);
    header->is_allocated = 1;
    header->before_alloc = 1; /* In a bump allocator, packed blocks always mean the previous is alive/boundary */
    header->custom_flags = custom_flags;
    header->handle = handle;

    /* Embed the footer at the very end of the block for O(1) reverse coalescing */
    PUT_FOOTER(header);
    uint32_t allocated_offset = bump->cur_index;
    
    bump->cur_index += size;
    bump->alloc_memory += size;

    return allocated_offset;
}
void bump_free(BumpAllocator* bump, uint32_t offset) {
    if (!bump || !bump->mem || offset >= bump->cur_index) return;

    BaseHeader* header = (BaseHeader*)(bump->mem + offset);
    if (!header->is_allocated) return;

    uint32_t block_size = HEADER_SIZE_TO_BYTES(header->size);
    
    header->is_allocated = 0;
    bump->alloc_memory -= block_size;

    /* Update the 'before_alloc' flag of the next physical block.
       This enables TLSF/Arena to coalesce correctly later. */
    uint32_t next_offset = offset + block_size;
    if (next_offset < bump->cur_index) {
        BaseHeader* next_header = (BaseHeader*)(bump->mem + next_offset);
        next_header->before_alloc = 0;
    }
}

uint32_t bump_defrag(BumpAllocator* bump) {
    if (!bump || !bump->mem) return 0;

    uint32_t read_offset = 0;
    uint32_t write_offset = 0;
    uint32_t starting_alloc = bump->alloc_memory;

    while (read_offset < bump->cur_index) {
        BaseHeader* header = (BaseHeader*)(bump->mem + read_offset);
        uint32_t block_size = HEADER_SIZE_TO_BYTES(header->size);
        
        int keep_block = 0;
        if (header->is_allocated) {
            /* Hand over to Nursery/Long-Lived arena logic to decide fate & update handles */
            keep_block = bump->defrag_cb(bump->arena_context, header, write_offset);
        }
        
        if (keep_block && read_offset != write_offset) {
            memmove(bump->mem + write_offset, bump->mem + read_offset, block_size);
        }
        
        if (keep_block) {
            /* Since we are sliding left, the previous block is guaranteed to be packed/alive */
            BaseHeader* moved_header = (BaseHeader*)(bump->mem + write_offset);
            moved_header->before_alloc = 1;
            write_offset += block_size;
        }
        read_offset += block_size;
    }

    bump->cur_index = write_offset;
    bump->alloc_memory = write_offset;

    return starting_alloc - bump->alloc_memory;
}

int bump_grow(BumpAllocator* bump, uint32_t requested_size) {
    if (!bump|| !bump->mem|| requested_size==0) return 0;
    
    uint32_t required = bump->mem_size + requested_size;
    if (required < bump->mem_size) return 0; // check if overflow
    uint32_t new_size = bump->mem_size;
    if (new_size == 0) new_size = 4096;

    while (new_size < required) {
        new_size <<= 1;
        if (new_size == 0) return 0;
    }

    uint8_t* new_mem = (uint8_t*)realloc(bump->mem, new_size);
    if (!new_mem) return 0;

    bump->mem = new_mem;
    bump->mem_size = new_size;

    return 1;
}