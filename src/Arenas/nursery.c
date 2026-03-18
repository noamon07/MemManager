#include "Arenas/nursery.h"
#include <stdlib.h>
#include <string.h>
#include "Arenas/handle.h"
#include "Arenas/promotion.h"

#define NURSERY_START_SIZE (4096)


#define ALIGNMENT (8)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define HEADER_SIZE_TO_SIZE(header_size) ((uint32_t)((header_size)<<3))
#define SIZE_TO_HEADER_SIZE(size) ((uint32_t)((size)>>3))
#define GET_INDEX(header) ((char*)header-(char*)nursery.mem)


static char initialized = 0;

typedef struct {
    uint32_t size:28,
             is_allocated:1,
             before_alloc:1,
             generation:2;
    uint32_t entry_index;
} BlockHeader;

static Nursery nursery;
int mm_nursery_grow(uint32_t requested_size);
void mm_nursery_merge_before(BlockHeader** _header);
void mm_trim_block(BlockHeader* header, uint32_t new_size);
void* mm_nursery_get_by_index(uint32_t index);
int mm_nursery_init()
{
    if(initialized)
        return 0;
    memset(&nursery,0,sizeof(Nursery));
    nursery.mem = calloc(NURSERY_START_SIZE,1);
    if(!nursery.mem)
        return 0;
    nursery.cur_index = 0;
    nursery.alloc_memory = 0;
    BlockHeader* header = (BlockHeader*)nursery.mem;
    header->size = SIZE_TO_HEADER_SIZE(NURSERY_START_SIZE);
    header->is_allocated = 0;
    header->before_alloc = 1;
    nursery.mem_size = NURSERY_START_SIZE;
    uint32_t* footer = (uint32_t*)(nursery.mem + NURSERY_START_SIZE - sizeof(uint32_t));
    *footer = NURSERY_START_SIZE;
    initialized = 1;
    return 1;
}
void mm_nursery_destroy()
{
    if(!initialized)
        return;
    if(nursery.mem)
        free(nursery.mem);
    memset(&nursery,0,sizeof(Nursery));
    initialized = 0;
}
void mm_nursery_defrag()
{
    BlockHeader* header;
    uint32_t offset = 0;
    for(;offset<nursery.mem_size;offset+=HEADER_SIZE_TO_SIZE(header->size))
    {
        header = (BlockHeader*)&nursery.mem[offset];
        if(header->is_allocated)
        {
            if(header->generation < 3)
            {
                header->is_allocated++;
            }
            else
            {
                nursury_promotion(header->entry_index,HEADER_SIZE_TO_SIZE(header->size));
            }
            
        }
        if(!header->before_alloc&&header->is_allocated)
        {
            uint32_t size = HEADER_SIZE_TO_SIZE(header->size);
            BlockHeader* header_backup = header;
            mm_nursery_merge_before(&header);
            memmove(header+1,header_backup+1,size-sizeof(BlockHeader));
            mm_trim_block(header,size);
            offset = GET_INDEX(header);
        }   
    }
}

uint32_t mm_malloc_nursery(uint32_t size, uint32_t** entry_index)
{
    mm_nursery_init();
    if(!initialized || size == 0)
        return INVALID_NURSERY_INDEX;
    if(size<sizeof(uint32_t))
        size = sizeof(uint32_t);
    size+=sizeof(BlockHeader);
    size = ALIGN(size);
    if(size>=nursery.mem_size-nursery.cur_index)
    {
        if((nursery.mem_size-nursery.alloc_memory)< size)
        {
            if(!mm_nursery_grow(size))
                return INVALID_NURSERY_INDEX;
        }
        else
            mm_nursery_defrag();
    }
    BlockHeader* to_alloc = (BlockHeader*)&nursery.mem[nursery.cur_index];
    to_alloc->before_alloc = 1;
    to_alloc->is_allocated = 1;
    to_alloc->generation = 0;
    to_alloc->size = SIZE_TO_HEADER_SIZE(size);
    nursery.cur_index += size;
    BlockHeader* new_cur = (BlockHeader*)&nursery.mem[nursery.cur_index];
    new_cur->before_alloc = 1;
    new_cur->is_allocated = 0;
    new_cur->size = SIZE_TO_HEADER_SIZE(nursery.mem_size-nursery.cur_index);
    *entry_index = (uint32_t*)&to_alloc->entry_index;
    nursery.alloc_memory+=size;
    return GET_INDEX(to_alloc);
}

void mm_nursery_merge_before(BlockHeader** _header)
{
    BlockHeader* header = *_header;
    if(header->before_alloc) return;

    uint32_t prev_size = *((uint32_t*)header-1);
    BlockHeader* prev_header = (BlockHeader*)((char*)header-prev_size);
    prev_header->size += header->size;
    prev_header->entry_index = header->entry_index;
    if(header->is_allocated)
    {
        HandleEntry* entry = handle_table_get_entry_by_index(prev_header->entry_index);
        entry->data.data_ptr.data_offset = GET_INDEX(prev_header);

        prev_header->is_allocated = 1;
        prev_header->generation = header->generation;

        nursery.alloc_memory += prev_size;
    }
    *_header = prev_header;
}
void mm_nursery_merge_after(BlockHeader* header)
{
    BlockHeader* next = (BlockHeader*)((char*)header+HEADER_SIZE_TO_SIZE(header->size));
    if(next->is_allocated) return;
    if (header->is_allocated) {
        nursery.alloc_memory += HEADER_SIZE_TO_SIZE(next->size);
    }
    header->size += next->size;
    BlockHeader* after_merged = (BlockHeader*)((char*)header + HEADER_SIZE_TO_SIZE(header->size));
    
    if ((char*)after_merged < (char*)nursery.mem + nursery.mem_size) {
        after_merged->before_alloc = header->is_allocated;
    }
}

void mm_free_nursery(data_pos data)
{
    if(!initialized)
        return;
    void* ptr = mm_nursery_get_by_index(data.data_offset);
    if(!ptr)
        return;
    BlockHeader* to_free = (BlockHeader*)ptr-1;
    if(!to_free->is_allocated)
        return;
    to_free->is_allocated = 0;
    nursery.alloc_memory-=HEADER_SIZE_TO_SIZE(to_free->size);
    BlockHeader* next = (BlockHeader*)((char*)to_free+HEADER_SIZE_TO_SIZE(to_free->size));
    next->before_alloc = 0;
    mm_nursery_merge_before(&to_free);
    mm_nursery_merge_after(to_free);
    uint32_t* end = (uint32_t*)((char*)to_free+HEADER_SIZE_TO_SIZE(to_free->size));
    end--;
    *end = HEADER_SIZE_TO_SIZE(to_free->size);
    uint32_t to_free_index = (char*)to_free-(char*)nursery.mem;
    if(nursery.cur_index<= to_free_index + HEADER_SIZE_TO_SIZE(to_free->size))
        nursery.cur_index = to_free_index;
}
void mm_trim_block(BlockHeader* header, uint32_t new_size)
{
    new_size = ALIGN(new_size);
    if(HEADER_SIZE_TO_SIZE(header->size)-new_size < ALIGN(sizeof(BlockHeader)+sizeof(uint32_t))) return;

    BlockHeader* split_header = (BlockHeader*)((char*)header+new_size);
    split_header->before_alloc = 1;
    split_header->is_allocated = 1;
    split_header->size = header->size-SIZE_TO_HEADER_SIZE(new_size);
    header->size = SIZE_TO_HEADER_SIZE(new_size);
    data_pos data;
    data.data_offset = (uint32_t)GET_INDEX(split_header);
    mm_free_nursery(data);
}

uint32_t mm_realloc_nursery(data_pos data, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index)
{
    if(!initialized)
        return INVALID_NURSERY_INDEX;
    void* ptr = mm_nursery_get_by_index(data.data_offset);
    if(!ptr)
        return INVALID_NURSERY_INDEX;
    new_size+=sizeof(BlockHeader);
    new_size = ALIGN(new_size);
    uint32_t before_size = 0, next_size = 0;
    BlockHeader* header = (BlockHeader*)ptr-1;
    if(new_size <= HEADER_SIZE_TO_SIZE(header->size))
    {
        mm_trim_block(header,new_size);
        *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
        return GET_INDEX(header);
    }
    BlockHeader* next = (BlockHeader*)((char*)header+HEADER_SIZE_TO_SIZE(header->size));
    if(!next->is_allocated)
        next_size = next->size;
    if(HEADER_SIZE_TO_SIZE(header->size+next_size) >= new_size)
    {
        mm_nursery_merge_after(header);
        mm_trim_block(header,new_size);
        *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
        header->generation=0;
        return GET_INDEX(header);
    }
    if(!header->before_alloc)
    {
        before_size = *((uint32_t*)header-1);
        if(before_size+HEADER_SIZE_TO_SIZE(header->size+next_size) >= new_size)
        {
            mm_nursery_merge_before(&header);
            mm_nursery_merge_after(header);
            memmove(header+1,ptr,ptr_size);
            mm_trim_block(header,new_size);
            ptr=header+1;
            *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
            header->generation=0;
            return GET_INDEX(header);
        }
    }
    void* new_ptr = mm_nursery_get_by_index(mm_malloc_nursery(new_size-sizeof(BlockHeader), entry_index));
    if(!new_ptr)
        return INVALID_NURSERY_INDEX;
    ptr = mm_nursery_get_by_index(data.data_offset);
    memmove(new_ptr,ptr,ptr_size);
    mm_free_nursery(data);
    BlockHeader* new_header = (BlockHeader*)new_ptr-1;
    new_header->generation = 0;
    return GET_INDEX(new_header);
}
uint32_t mm_calloc_nursery(uint32_t size, uint32_t** entry_index)
{
    uint32_t index = mm_malloc_nursery(size, entry_index);
    if(index!=INVALID_NURSERY_INDEX)
    {
        void* ptr = mm_nursery_get_by_index(index);
        memset(ptr,0,size);
    }
    return index;
}
int mm_nursery_grow(uint32_t requested_size)
{
    requested_size+=nursery.mem_size;
    uint32_t calc_size = nursery.mem_size;
    while(calc_size <= requested_size)
    {
        calc_size*=2;
    }
    void* new_mem = realloc(nursery.mem,calc_size);
    if(!new_mem)
        return 0;
    nursery.mem = new_mem;
    nursery.mem_size = calc_size;
    BlockHeader * cur_header = (BlockHeader*)&nursery.mem[nursery.cur_index];
    cur_header->size=SIZE_TO_HEADER_SIZE(nursery.mem_size-nursery.cur_index);
    cur_header->is_allocated = 0;
    return 1;
}
void* mm_nursery_get_by_index(uint32_t index)
{
    if (index == INVALID_NURSERY_INDEX)
        return NULL;
    return &nursery.mem[index+sizeof(BlockHeader)];
}
void* mm_nursery_get(HandleEntry* entry)
{
    if (entry->stratigy_id!=ALLOC_TYPE_NURSERY)
        return NULL;
    return mm_nursery_get_by_index(entry->data.data_ptr.data_offset);
}

Nursery* mm_get_nursery_instance(void)
{
    return &nursery;
}
