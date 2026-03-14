#pragma warning(disable:4996)
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "heap.h"

#define ALIGNMENT 4
#define MINBLOCKSIZE (HEADER_SIZE + ALIGNMENT)
// The mask for alignment: ensures the size is a multiple of 4 (i.e., bits 0 and 1 are 0) by doing this:.
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x3) 

// Flags packed into the lowest bits of the 4-byte size_t header
#define ALLOCATED_MASK 0x1       // Bit 0: Is the current block allocated? (A)
#define PREV_ALLOCATED_MASK 0x2  // Bit 1: Is the previous block allocated? (P)

typedef struct {
    size_t size_and_flags; 
} BlockHeader;
void split(BlockHeader* bp, size_t asize);
void coalesce(BlockHeader** bp);
// Define the size of the header for calculations
// HEADER_SIZE will be ALIGN(4) which is 4 bytes.
#define HEADER_SIZE ALIGN(sizeof(BlockHeader))

// --- Helper Macros for Header Manipulation (32-bit specific mask) ---

// Get the actual size of the block (by masking off the flags)
#define GET_SIZE(p) ((p)->size_and_flags & ~0x3)

#define SET_SIZE(p, asize) (p)->size_and_flags = (asize) | ((p)->size_and_flags & (ALLOCATED_MASK+PREV_ALLOCATED_MASK))

// Check the Allocated status of the current block
#define IS_ALLOCATED(p) ((p)->size_and_flags & ALLOCATED_MASK)

// Check the Previous Allocated status
#define IS_PREV_ALLOCATED(p) ((p)->size_and_flags & PREV_ALLOCATED_MASK)

// Set the Allocated flag
#define SET_ALLOCATED(p) ((p)->size_and_flags |= ALLOCATED_MASK)

// Clear the Allocated flag
#define CLEAR_ALLOCATED(p) ((p)->size_and_flags &= ~ALLOCATED_MASK)

// Set the Previous Allocated flag
#define SET_PREV_ALLOCATED(p) ((p)->size_and_flags |= PREV_ALLOCATED_MASK)

// Clear the Previous Allocated flag
#define CLEAR_PREV_ALLOCATED(p) ((p)->size_and_flags &= ~PREV_ALLOCATED_MASK)

#define GET_NEXT_BLOCK_HEADER(p) ((BlockHeader*)((char*)(p) + GET_SIZE(p)))

#define GET_EPILOGUE_BP() (heap_end - HEADER_SIZE)
// Define a reasonable size for our simulated heap in bytes.
// 1 Megabyte for testing. Must be a multiple of ALIGNMENT.
#define HEAP_SIZE (1024 * 1024) 

#define PUT_FOOTER(bp) *(size_t*)((char*)(bp) + GET_SIZE(bp) - sizeof(size_t))=GET_SIZE(bp)

// Global memory arena simulation (treated as the contiguous heap memory)
static char heap_memory[HEAP_SIZE];

// Global pointer to the start of the usable heap block
static BlockHeader* heap_start = NULL;
// Global pointer to the end of the usable heap memory (one byte past the last address)
static void* heap_end = NULL;

// --- Core Heap Initialization Function ---

int mm_init(void) {
    // 1. Initialize global pointers
    heap_start = (BlockHeader*)heap_memory;
    heap_end = (void*)(heap_memory + HEAP_SIZE);
    size_t initial_free_size = HEAP_SIZE - HEADER_SIZE;
    // 3. Setup the first block header
    heap_start->size_and_flags = ALIGN(initial_free_size);
    SET_PREV_ALLOCATED(heap_start);
    PUT_FOOTER(heap_start);

    BlockHeader *epilogue_bp = GET_EPILOGUE_BP();
    epilogue_bp->size_and_flags = ALLOCATED_MASK;
    

    return 0; // Success
}

BlockHeader* find_fit(size_t asize)
{
    BlockHeader *bp = heap_start;

    while((void*)bp < GET_EPILOGUE_BP())
    {
        if(!IS_ALLOCATED(bp) && GET_SIZE(bp) >= asize)
        {
            return bp;
        }
        bp = GET_NEXT_BLOCK_HEADER(bp);
    }
    return NULL;
}

void* mm_malloc(size_t size)
{
    if(size==0)
    {
        return NULL;
    }
    size_t asize = ALIGN(size + HEADER_SIZE);
    BlockHeader* bp = find_fit(asize);
    if(bp != NULL)
    {
        split(bp, asize);
        SET_ALLOCATED(bp);
        return (void*)((char*)bp+HEADER_SIZE);
    }
    return NULL;
}
void split(BlockHeader* bp, size_t asize)
{
    size_t size = GET_SIZE(bp);
    BlockHeader* new_bp;
    if(size-asize >= MINBLOCKSIZE)
    {
        new_bp = (BlockHeader*)((char*)bp+asize);
        SET_SIZE(new_bp, size-asize);
        CLEAR_ALLOCATED(new_bp);
        SET_PREV_ALLOCATED(new_bp);
        PUT_FOOTER(new_bp);
        new_bp = (BlockHeader*)((char*)new_bp+size-asize);
        CLEAR_PREV_ALLOCATED(new_bp);
        SET_SIZE(bp, asize);
    }
    else
    {
        new_bp = (BlockHeader*)((char*)bp+size);
        SET_PREV_ALLOCATED(new_bp);
        SET_SIZE(bp,size);
    }
}

void coalesce(BlockHeader** bp)
{
    size_t size = GET_SIZE(*bp);
    BlockHeader* next_bp = GET_NEXT_BLOCK_HEADER(*bp);
    if(!IS_ALLOCATED(next_bp))
    {
        size += GET_SIZE(next_bp);
    }
    if(!IS_PREV_ALLOCATED(*bp))
    {
        *bp = (BlockHeader*)((char*)(*bp)-*(size_t*)((char*)(*bp)-sizeof(size_t)));
        size += GET_SIZE(*bp);
    }
    SET_SIZE(*bp, size);
}

void mm_free(void* ptr)
{
    if(ptr == NULL)
    {
        return;
    }
    BlockHeader* bp = (BlockHeader*)((char*)ptr - HEADER_SIZE);
    CLEAR_ALLOCATED(bp);
    coalesce(&bp);
    PUT_FOOTER(bp);
    BlockHeader* new_bp = GET_NEXT_BLOCK_HEADER(bp);
    CLEAR_PREV_ALLOCATED(new_bp);
}
void* mm_realloc(void* ptr, size_t size)
{
    if(ptr == NULL)
    {
        return mm_malloc(size);
    }
    if(size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    BlockHeader* bp = (BlockHeader*)((char*)ptr-HEADER_SIZE);
    size_t new_asize = ALIGN(size + HEADER_SIZE);
    size_t current_size = GET_SIZE(bp);
    if(current_size >= new_asize)
    {
        split(bp, new_asize);
        return ptr;
    }
    else
    {
        BlockHeader* nextbp = GET_NEXT_BLOCK_HEADER(bp);
        size_t next_size = GET_SIZE(nextbp);
        if(!IS_ALLOCATED(nextbp) && current_size+next_size >= new_asize)
        {
            SET_SIZE(bp, current_size + next_size);
            split(bp,new_asize);
            return ptr;        
        }
        else
        {
            void* new_ptr = mm_malloc(size);
            if(new_ptr == NULL)
            {
                return NULL;
            }
            size_t copy_size = current_size - HEADER_SIZE; 
            if (size < copy_size) copy_size = size;
            memcpy(new_ptr, ptr, copy_size);
            mm_free(ptr);
            return new_ptr;
        }
    }
}
int mm_checkheap(int verbose) 
{
    BlockHeader *bp = heap_start;

    if (verbose) printf("--- Heap Check ---\n");

    // 1. Check Epilogue Alignment (Basic sanity)
    if ((size_t)heap_end % ALIGNMENT != 0) {
        printf("Error: Heap end is not aligned\n");
        return 0;
    }

    while ((void*)bp < GET_EPILOGUE_BP()) 
    {
        if (verbose) {
            printf("Block at %p: Size %u, Alloc %d, PrevAlloc %d\n", 
                   bp, GET_SIZE(bp), IS_ALLOCATED(bp), IS_PREV_ALLOCATED(bp));
        }

        // Check 1: Alignment
        if ((size_t)bp % ALIGNMENT != 0) {
            printf("Error: Block %p is not aligned\n", bp);
            return 0;
        }

        // Check 2: Header matches Footer (only if block is FREE)
        if (!IS_ALLOCATED(bp)) {
            size_t footer_size = *(size_t*)((char*)bp + GET_SIZE(bp) - sizeof(size_t));
            // Mask out the size from the header
            if (GET_SIZE(bp) != (footer_size & ~0x3)) {
                printf("Error: Header/Footer mismatch at %p\n", bp);
                return 0;
            }
        }

        // Check 3: Coalescing (No two consecutive free blocks)
        BlockHeader* next_bp = GET_NEXT_BLOCK_HEADER(bp);
        if (!IS_ALLOCATED(bp) && !IS_ALLOCATED(next_bp)) {
            printf("Error: Consecutive free blocks escaping coalesce at %p\n", bp);
            return 0;
        }
        
        // Check 4: Prev_Alloc consistency
        // The current block's PREV_ALLOC bit must match the previous block's status.
        // (This requires looking "behind", strictly speaking, but checking forward is easier)
        if (IS_ALLOCATED(bp)) {
            // If I am allocated, the NEXT block must have PREV_ALLOC set.
            // Note: This logic depends on whether next_bp is the Epilogue.
            // The epilogue should have PREV_ALLOC set if the last block is allocated.
            if (!IS_PREV_ALLOCATED(next_bp)) {
                 printf("Error: Block at %p is Alloc, but Next Block says Prev is Free\n", bp);
                 return 0;
            }
        } else {
             // If I am free, the NEXT block must have PREV_ALLOC cleared.
            if (IS_PREV_ALLOCATED(next_bp)) {
                 printf("Error: Block at %p is Free, but Next Block says Prev is Alloc\n", bp);
                 return 0;
            }
        }

        bp = next_bp;
    }
    
    if (verbose) printf("--- Heap Check Passed ---\n");
    return 1;
}