#include "Arenas/general.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#define GENERAL_START_POWER_OF_2 (12)//4096
#define GENERAL_START_SIZE (1<<GENERAL_START_POWER_OF_2)
#define ALIGNMENT_BITS (3)
#define ALIGNMENT (1<<ALIGNMENT_BITS)
#define MIN_SIZE (ALIGN(sizeof(FreeBlockHeader)+sizeof(uint32_t)))

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HEADER_SIZE_TO_SIZE(header_size) ((uint32_t)((header_size)<<ALIGNMENT_BITS))
#define SIZE_TO_HEADER_SIZE(size) ((uint32_t)((size)>>ALIGNMENT_BITS))
#define GET_INDEX(header) ((char*)header-(char*)general.mem)
#define PUT_FOOTER(header) (*(uint32_t*)((char*)(header) + HEADER_SIZE_TO_SIZE((header)->size) - sizeof(uint32_t))= HEADER_SIZE_TO_SIZE((header)->size));



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


static General general;
static char initialized = 0;
void mm_general_remove_block(FreeBlockHeader* block);
void mm_general_split_block(BlockHeader* block, uint32_t size);
void* mm_general_get(HandleEntry* entry);
void* mm_general_get_by_index(uint32_t index);

// Helper function to map a size to its FL and SL indices
static void mm_general_mapping(uint32_t size, uint32_t *fl, uint32_t *sl) {
    // 1. First Level: Find the highest bit set
    *fl = 31 - __builtin_clz(size);

    // 2. Second Level: Extract the next 4 bits
    *sl = (size >> (*fl - 4)) & 0xF;
}

void mm_general_register_block(FreeBlockHeader* block) {
    // 1. Link the block into the array;
    uint32_t fl,sl;
    mm_general_mapping(HEADER_SIZE_TO_SIZE(block->header.size), &fl, &sl);
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
    PUT_FOOTER((BlockHeader*)header);
    mm_general_register_block(header);
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
        fl = __builtin_ctz(fl_map);
        
        // Since this entire drawer holds blocks larger than we need, 
        // we can grab the smallest available folder inside it.
        sl = __builtin_ctz(general.sl_bitmap[fl]);
    }
    else{
        // We found a block in our original drawer! 
        sl = __builtin_ctz(sl_map);
    }
    // 4. Grab the block and remove it from the free list
    FreeBlockHeader* block = (FreeBlockHeader*)(general.mem + general.blocks[fl][sl]);
    mm_general_remove_block(block);
    general.alloc_memory += HEADER_SIZE_TO_SIZE(block->header.size);
    mm_general_split_block((BlockHeader*)block, size);

    // 6. Mark it as allocated!
    block->header.is_allocated = 1;
    block->header.generation = 0;

    // 7. Update the physical neighbor so it knows we are no longer free
    BlockHeader* next_physical = (BlockHeader*)((char*)block + HEADER_SIZE_TO_SIZE(block->header.size));
    if ((char*)next_physical < (char*)general.mem + general.mem_size) {
        next_physical->before_alloc = 1; 
    }
    *entry_index = (uint32_t*)&block->header.entry_index;

    return GET_INDEX(&block->header);
}
void mm_general_remove_block(FreeBlockHeader* block)
{
    uint32_t fl,sl;
    mm_general_mapping(HEADER_SIZE_TO_SIZE(block->header.size), &fl, &sl);
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
void mm_general_split_block(BlockHeader* block, uint32_t size)
{
    uint32_t block_size = HEADER_SIZE_TO_SIZE(block->size);
    if (block_size - size >= MIN_SIZE) {
        FreeBlockHeader* new_block = (FreeBlockHeader*)((char*)block + size);
        new_block->header.before_alloc = 1;
        new_block->header.is_allocated = 1;
        new_block->header.size = SIZE_TO_HEADER_SIZE(block_size - size);
        block->size = SIZE_TO_HEADER_SIZE(size);
        data_pos data;
        data.data_offset = (uint32_t)GET_INDEX(new_block);
        mm_free_general(data);
    }
}

void mm_general_merge_before(BlockHeader** _header)
{
    BlockHeader* header = *_header;
    if(header->before_alloc) return;

    uint32_t prev_size = *((uint32_t*)header-1);
    BlockHeader* prev_header = (BlockHeader*)((char*)header-prev_size);
    mm_general_remove_block((FreeBlockHeader*)prev_header);
    prev_header->size += header->size;
    prev_header->entry_index = header->entry_index;
    if(header->is_allocated)
    {
        HandleEntry* entry = handle_table_get_entry_by_index(prev_header->entry_index);
        entry->data.data_ptr.data_offset = GET_INDEX(prev_header);

        prev_header->is_allocated = 1;
        prev_header->generation = header->generation;

        general.alloc_memory += prev_size;
    }
    *_header = prev_header;
}
void mm_general_merge_after(BlockHeader* header)
{
    BlockHeader* next = (BlockHeader*)((char*)header+HEADER_SIZE_TO_SIZE(header->size));
    if((char*)next >= (char*)general.mem + general.mem_size ||next->is_allocated) return;
    mm_general_remove_block((FreeBlockHeader*)next);
    if (header->is_allocated) {
        general.alloc_memory += HEADER_SIZE_TO_SIZE(next->size);
    }
    header->size += next->size;
    BlockHeader* after_merged = (BlockHeader*)((char*)header + HEADER_SIZE_TO_SIZE(header->size));
    
    if ((char*)after_merged < (char*)general.mem + general.mem_size) {
        after_merged->before_alloc = header->is_allocated;
    }
}
void mm_free_general(data_pos data)
{
    if(!initialized)
        return;
    BlockHeader* header = (BlockHeader*)(general.mem + data.data_offset);
    if(!header->is_allocated)
        return;
    header->is_allocated = 0;
    general.alloc_memory-=HEADER_SIZE_TO_SIZE(header->size);
    BlockHeader* next = (BlockHeader*)((char*)header+HEADER_SIZE_TO_SIZE(header->size));
    if ((char*)next < (char*)general.mem + general.mem_size)
    {
        next->before_alloc = 0;
    }
    mm_general_merge_before(&header);
    mm_general_merge_after(header);
    PUT_FOOTER(header);
    mm_general_register_block((FreeBlockHeader*)header);
}
uint32_t mm_realloc_general(data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index)
{
    mm_general_init();
    if(!initialized)
        return INVALID_GENERAL_INDEX;
    void* ptr = mm_general_get_by_index(data.data_offset);
    if(!ptr)
        return INVALID_GENERAL_INDEX;
    new_size+=sizeof(BlockHeader);
    if(new_size<sizeof(uint32_t)+sizeof(FreeBlockHeader))
        new_size = sizeof(uint32_t)+sizeof(FreeBlockHeader);   
    new_size = ALIGN(new_size);
    uint32_t before_size = 0, next_size = 0;
    BlockHeader* header = (BlockHeader*)ptr-1;
    if(new_size <= HEADER_SIZE_TO_SIZE(header->size))
    {
        mm_general_split_block(header,new_size);
        return GET_INDEX(header);
    }
    BlockHeader* next = (BlockHeader*)((char*)header+HEADER_SIZE_TO_SIZE(header->size));
    if((char*)next < (char*)general.mem + general.mem_size && !next->is_allocated)
        next_size = next->size;
    if(HEADER_SIZE_TO_SIZE(header->size+next_size) >= new_size)
    {
        mm_general_merge_after(header);
        mm_general_split_block(header,new_size);
        header->generation=0;
        return GET_INDEX(header);
    }
    if(!header->before_alloc)
    {
        before_size = *((uint32_t*)header-1);
        if(before_size+HEADER_SIZE_TO_SIZE(header->size+next_size) >= new_size)
        {
            mm_general_merge_before(&header);
            mm_general_merge_after(header);
            memmove(header+1,ptr,ptr_size);
            mm_general_split_block(header,new_size);
            ptr=header+1;
            *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
            header->generation=0;
            return GET_INDEX(header);
        }
    }
    void* new_ptr = mm_general_get_by_index(mm_malloc_general(new_size-sizeof(BlockHeader), entry_index));
    if(!new_ptr)
        return INVALID_GENERAL_INDEX;
    ptr = mm_general_get_by_index(data.data_offset);
    memmove(new_ptr,ptr,ptr_size);
    mm_free_general(data);
    BlockHeader* new_header = (BlockHeader*)new_ptr-1;
    new_header->generation = 0;
    return GET_INDEX(new_header);
}
uint32_t mm_calloc_general(uint32_t size, uint32_t** entry_index)
{
    uint32_t index = mm_malloc_general(size, entry_index);
    if(index!=INVALID_GENERAL_INDEX)
    {
        void* ptr = mm_general_get_by_index(index);
        memset(ptr,0,size);
    }
    return index;
}
void* mm_general_get_by_index(uint32_t index)
{
    if (index == INVALID_GENERAL_INDEX)
        return NULL;
    return &general.mem[index+sizeof(BlockHeader)];
}
void* mm_general_get(HandleEntry* entry)
{
    if (entry->stratigy_id!=ALLOC_TYPE_GENERAL)
        return NULL;
    return mm_general_get_by_index(entry->data.data_ptr.data_offset);
}
General* mm_get_general_instance(void)
{
    return &general;
}