#include "Arenas/nursery.h"
#include <stdlib.h>
#include <string.h>
#include "Arenas/handle.h"
#define NURSERY_START_SIZE (4096)

static char initialized = 0;
typedef struct {
    uint32_t size:29,
             is_allocated:1,
             before_alloc:1,
             unused:1;
    uint32_t entry_index;
} BlockHeader;

static Nursery nursery;
int mm_nursery_grow(uint32_t requested_size);
void mm_nursery_merge_before(BlockHeader** _header);
void mm_trim_block(BlockHeader* header, uint32_t new_size);
int mm_nursery_init()
{
    if(initialized)
        return 0;
    memset(&nursery,0,sizeof(Nursery));
    nursery.mem = calloc(NURSERY_START_SIZE,1);
    if(!nursery.mem)
        return 0;
    BlockHeader* header = (BlockHeader*)nursery.mem;
    header->size = NURSERY_START_SIZE;
    header->is_allocated = 0;
    header->before_alloc = 1;
    nursery.mem_size = NURSERY_START_SIZE;
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
void mm_nursery_reset()
{
    if(!initialized)
        return;
    // Reset the bump pointer to the beginning of the nursery.
    nursery.cur_index = 0;
    // Mark the entire nursery as a single, large, free block.
    BlockHeader* header = (BlockHeader*)nursery.mem;
    header->size = nursery.mem_size;
    header->is_allocated = 0;
}
void mm_nursery_defrag()
{
    BlockHeader* header;
    uint32_t offset = 0;
    for(;offset<nursery.mem_size;offset+=header->size)
    {
        header = (BlockHeader*)&nursery.mem[offset];
        if(header->is_allocated && !header->before_alloc)
        {
            uint32_t size = header->size;
            BlockHeader* header_backup = header;
            mm_nursery_merge_before(&header);
            memmove(header+1,header_backup+1,size-sizeof(BlockHeader));
            mm_trim_block(header,size);
        }   
    }
}

void* mm_malloc_nursery(uint32_t size, uint32_t** entry_index)
{
    mm_nursery_init();
    if(!initialized)
        return NULL;
    if(size+sizeof(BlockHeader)>=nursery.mem_size-nursery.cur_index)
    {
        if(!mm_nursery_grow(size+sizeof(BlockHeader)))
            return NULL;
    }
    BlockHeader* to_alloc = (BlockHeader*)&nursery.mem[nursery.cur_index];
    to_alloc->before_alloc = 1;
    to_alloc->is_allocated = 1;
    to_alloc->size = size+sizeof(BlockHeader);
    nursery.cur_index += to_alloc->size;
    BlockHeader* new_cur = (BlockHeader*)&nursery.mem[nursery.cur_index];
    new_cur->before_alloc = 1;
    new_cur->is_allocated = 0;
    new_cur->size = nursery.mem_size-nursery.cur_index;
    *entry_index = (uint32_t*)&to_alloc->entry_index;
    return to_alloc+1;
}

void mm_nursery_merge_before(BlockHeader** _header)
{
    BlockHeader* header = *_header;
    if(header->before_alloc) return;

    uint32_t prev_size = *(uint32_t*)header-1;
    BlockHeader* prev_header = (BlockHeader*)((char*)header-prev_size);
    prev_header->size += header->size;
    prev_header->entry_index = header->entry_index;
    HandleEntry* entry = handle_table_get_entry_by_index(prev_header->entry_index);
    entry->data.ptr = prev_header+1;
    *_header = prev_header;
}
void mm_nursery_merge_after(BlockHeader* header)
{
    BlockHeader* next = (BlockHeader*)((char*)header+header->size);
    if(next->is_allocated) return;
    header->size += next->size;
}

void mm_free_nursery(void* ptr)
{
    if(!initialized)
        return;
    BlockHeader* to_free = (BlockHeader*)ptr-1;
    to_free->is_allocated = 0;
    BlockHeader* next = (BlockHeader*)((char*)to_free+to_free->size);
    next->before_alloc = 0;
    mm_nursery_merge_before(&to_free);
    mm_nursery_merge_after(to_free);
    uint32_t* end = (uint32_t*)((char*)to_free+to_free->size);
    end--;
    *end = to_free->size;
    uint32_t to_free_index = (char*)to_free-(char*)nursery.mem;
    if(nursery.cur_index< to_free_index + to_free->size)
        nursery.cur_index = to_free_index;
}
void mm_trim_block(BlockHeader* header, uint32_t new_size)
{
    if(header->size-new_size < sizeof(BlockHeader)+sizeof(uint32_t)) return;

    BlockHeader* split_header = (BlockHeader*)((char*)header+new_size);
    split_header->before_alloc = 1;
    split_header->is_allocated = 1;
    split_header->size = header->size-new_size;
    header->size = new_size;
    mm_free_nursery(split_header);
}
void* mm_realloc_nursery(void* ptr, uint32_t ptr_size, uint32_t new_size, uint32_t** entry_index)
{
    if(!initialized)
        return NULL;
    new_size+=sizeof(BlockHeader);
    uint32_t before_size = 0, next_size = 0;
    BlockHeader* header = (BlockHeader*)ptr-1;
    if(new_size <= header->size)
    {
        mm_trim_block(header,new_size);
        *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
        return ptr;
    }
    BlockHeader* next = (BlockHeader*)((char*)header+header->size);
    next_size = next->size;
    if(header->size+next_size >= new_size)
    {
        mm_nursery_merge_after(header);
        mm_trim_block(header,new_size);
        *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
        return ptr;
    }
    if(!header->before_alloc)
    {
        before_size = *((uint32_t*)header-1);
        if(before_size+header->size+next_size >= new_size)
        {
            mm_nursery_merge_before(&header);
            mm_nursery_merge_after(header);
            mm_trim_block(header,new_size);
            memmove(header+1,ptr,ptr_size);
            ptr=header+1;
            *entry_index = (uint32_t*)&((BlockHeader*)ptr-1)->entry_index;
            return ptr;
        }
    }
    void* new_ptr = mm_malloc_nursery(new_size, entry_index);
    if(!new_ptr)
        return NULL;
    memmove(new_ptr,ptr,ptr_size);
    mm_free_nursery(ptr);
    return new_ptr;
}
void* mm_calloc_nursery(uint32_t size, uint32_t** entry_index)
{
    void* ptr = mm_malloc_nursery(size, entry_index);
    if(!ptr)
        return NULL;
    memset(ptr,0,size);
    return ptr;
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
    cur_header->size=nursery.mem_size-nursery.cur_index;
    cur_header->is_allocated = 0;
    return 1;
}