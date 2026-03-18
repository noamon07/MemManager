#include "Arenas/general.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define GENERAL_START_POWER_OF_2 (12)//4096
#define GENERAL_START_SIZE (1<<GENERAL_START_POWER_OF_2)

#define ALIGNMENT (8)
#define MIN_SIZE (24)

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HEADER_SIZE_TO_SIZE(header_size) ((uint32_t)((header_size)<<3))
#define SIZE_TO_HEADER_SIZE(size) ((uint32_t)((size)>>3))
#define GET_INDEX(header) ((char*)header-(char*)general.mem)

#define TLSF_FL_INDEX_MAX 32 //128 MB
#define TLSF_SL_INDEX_COUNT 16

typedef struct {
    uint32_t size:28,
             is_allocated:1,
             before_alloc:1,
             generation:2;
    uint32_t entry_index;
} BlockHeader;

typedef struct {
    BlockHeader header;
    uint32_t prev_free;
    uint32_t next_free;
} FreeBlockHeader;
typedef struct General {
    uint8_t* mem;                  /* Pointer to the start of the managed memory pool */
    uint32_t mem_size;             /* Total size of the memory pool */
    uint32_t alloc_memory;         /* Currently allocated memory in bytes */
    
    /* TLSF-specific control structures */
    uint32_t fl_bitmap;                                         /* First-Level bitmap */
    uint32_t sl_bitmap[TLSF_FL_INDEX_MAX];                      /* Second-Level bitmaps */
    uint32_t blocks[TLSF_FL_INDEX_MAX][TLSF_SL_INDEX_COUNT];  /* Heads of free block lists */
} General;

static General general;
static char initialized = 0;
void mm_general_remove_block(uint32_t fl, uint32_t sl, FreeBlockHeader* block);
void mm_general_split_block(FreeBlockHeader* block, uint32_t size);

int count_leading_zeros(uint32_t n) {
    // Counts the number of zeros from the most significant bit (MSB) to the first set (1) bit.
    if (n == 0) {
        return 32; // Standard behavior for 32-bit integer input of zero
    }
    // Using GCC/Clang built-in for count leading zeros
    return __builtin_clz(n);
}
// Helper function to map a size to its FL and SL indices
static void mm_general_mapping(uint32_t size, uint32_t *fl, uint32_t *sl) {
    // 1. First Level: Find the highest bit set
    *fl = 31 - __builtin_clz(size);

    // 2. Second Level: Extract the next 4 bits
    *sl = (size >> (*fl - 4)) & 0xF;
}

void mm_general_register_block(uint32_t fl, uint32_t sl, FreeBlockHeader* block) {
    // 1. Link the block into the array;
    block->prev_free = INVALID_GENERAL_INDEX;
    block->next_free = general.blocks[fl][sl];
    if (general.blocks[fl][sl] != INVALID_GENERAL_INDEX) {
        FreeBlockHeader* old_head = (FreeBlockHeader*)(general.mem + general.blocks[fl][sl]);
        old_head->prev_free = GET_INDEX(block);
    }
    general.blocks[fl][sl] = GET_INDEX(block);

    // 2. Turn ON the bit in the First-Level bitmap
    // This says: "Hey, drawer 'fl' has something in it!"
    general.fl_bitmap |= (1U << fl);

    // 3. Turn ON the bit in the Second-Level bitmap
    // This says: "Inside drawer 'fl', folder 'sl' specifically has the block!"
    general.sl_bitmap[fl] |= (1U << sl);
}
int mm_general_init()
{
    if(initialized)
        return 0;
    memset(&general,0,sizeof(general));
    general.mem = calloc(GENERAL_START_SIZE,1);
    if(!general.mem)
        return 0;
    for(uint32_t i = 0; i < TLSF_FL_INDEX_MAX; i++)
    {
        for(uint32_t j = 0; j < TLSF_SL_INDEX_COUNT; j++)
        {
            general.blocks[i][j] = INVALID_GENERAL_INDEX;
        }
    }
        
    general.mem_size = GENERAL_START_SIZE;
    FreeBlockHeader* header = (FreeBlockHeader*)general.mem;
    header->header.size = SIZE_TO_HEADER_SIZE(GENERAL_START_SIZE);
    header->header.is_allocated = 0;
    header->header.before_alloc = 1;
    header->prev_free = INVALID_GENERAL_INDEX;
    header->next_free = INVALID_GENERAL_INDEX;
    uint32_t* footer = (uint32_t*)(general.mem + GENERAL_START_SIZE - sizeof(uint32_t));
    *footer = GENERAL_START_SIZE;
    uint32_t fl,sl;
    mm_general_mapping(HEADER_SIZE_TO_SIZE(header->header.size), &fl, &sl);
    mm_general_register_block(fl,sl,header);
    initialized = 1;
    return 1;
}
void mm_general_destroy()
{
    if(!initialized)
        return;
    if(general.mem)
        free(general.mem);
    memset(&general,0,sizeof(general));
    initialized = 0;
}
uint32_t mm_malloc_general(uint32_t size, uint32_t** entry_index)
{
    mm_general_init();
    if(!initialized || size == 0)
        return INVALID_GENERAL_INDEX;
    size+=sizeof(BlockHeader);
    if(size<sizeof(uint32_t)+sizeof(FreeBlockHeader))
        size = sizeof(uint32_t)+sizeof(FreeBlockHeader);   
    size = ALIGN(size);
    
    uint32_t fl, sl;
    mm_general_mapping(size, &fl, &sl);

    uint32_t target_fl = fl;
    uint32_t target_sl = sl;
    uint32_t sl_map = general.sl_bitmap[fl] & (~0U << sl);
    if(!sl_map)
    {
        // Nothing big enough in this drawer. Look at larger drawers!
        // Mask out everything smaller than or equal to our current FL
        uint32_t fl_map = general.fl_bitmap & (~0U << (fl + 1));
        
        if (!fl_map) {
            // No larger drawers have ANY free blocks. Out of memory!
            // (In the future, you could call a mm_general_grow() here)
            return INVALID_GENERAL_INDEX; 
        }
        
        // Find the lowest available bit in the FL bitmap
        target_fl = __builtin_ctz(fl_map);
        
        // Since this entire drawer holds blocks larger than we need, 
        // we can grab the smallest available folder inside it.
        target_sl = __builtin_ctz(general.sl_bitmap[target_fl]);
    }
    else{
        // We found a block in our original drawer! 
        target_sl = __builtin_ctz(sl_map);
    }
    // 4. Grab the block and remove it from the free list
    FreeBlockHeader* block = (FreeBlockHeader*)(general.mem + general.blocks[target_fl][target_sl]);
    mm_general_remove_block(target_fl, target_sl, block);
    mm_general_split_block(block, size);

    // 6. Mark it as allocated!
    block->header.is_allocated = 1;
    block->header.generation = 0;

    // 7. Update the physical neighbor so it knows we are no longer free
    BlockHeader* next_physical = (BlockHeader*)((char*)block + HEADER_SIZE_TO_SIZE(block->header.size));
    if ((char*)next_physical < (char*)general.mem + general.mem_size) {
        next_physical->before_alloc = 0; 
    }
    *entry_index = (uint32_t*)&block->header.entry_index;
    general.alloc_memory += HEADER_SIZE_TO_SIZE(block->header.size);

    return GET_INDEX(&block->header);
}
void mm_general_remove_block(uint32_t fl, uint32_t sl, FreeBlockHeader* block)
{
    if(block->prev_free != INVALID_GENERAL_INDEX)
    {
        FreeBlockHeader* prev_block = (FreeBlockHeader*)(general.mem + block->prev_free);
        prev_block->next_free = block->next_free;
    }
    else
    {
        general.blocks[fl][sl] = block->next_free;
        if(general.blocks[fl][sl] == INVALID_GENERAL_INDEX)
        {
            general.sl_bitmap[fl] &= ~(1U << sl);
            if(!general.sl_bitmap[fl])
            {
                general.fl_bitmap &= ~(1U << fl);
            }
        }
    }
    if(block->next_free != INVALID_GENERAL_INDEX)
    {
        FreeBlockHeader* next_block = (FreeBlockHeader*)(general.mem + block->next_free);
        next_block->prev_free = block->prev_free;
    }
}
void mm_general_split_block(FreeBlockHeader* block, uint32_t size)
{
    uint32_t block_size = HEADER_SIZE_TO_SIZE(block->header.size);
    if (block_size - size >= MIN_SIZE) {
        block->header.size = SIZE_TO_HEADER_SIZE(size);
        FreeBlockHeader* new_block = (FreeBlockHeader*)((char*)block + size);
        new_block->header.size = SIZE_TO_HEADER_SIZE(block_size - size);
        new_block->header.is_allocated = 0;
        new_block->header.before_alloc = 1;
        uint32_t fl, sl;
        mm_general_mapping(HEADER_SIZE_TO_SIZE(new_block->header.size), &fl, &sl);
        mm_general_register_block(fl, sl, new_block);
        uint32_t* footer = (uint32_t*)((char*)new_block + HEADER_SIZE_TO_SIZE(new_block->header.size) - sizeof(uint32_t));
        *footer = HEADER_SIZE_TO_SIZE(new_block->header.size);
    }
}
void mm_free_general(data_pos data);
uint32_t mm_realloc_general(data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index);
uint32_t mm_calloc_general(uint32_t size, uint32_t** entry_index);
void* mm_general_get(HandleEntry* entry)
{
    (void)entry;
    return NULL;
}
General* mm_get_general_instance(void);