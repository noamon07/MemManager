#include "Strategies/nursery.h"
#include "Strategies/promotion.h"
#include <string.h>
#include <stdio.h>

#define NURSERY_START_SIZE (4096)

static Nursery nursery;
static uint8_t initialized = 0;

static int nursery_defrag_callback(void* arena_context, BaseHeader* header, uint32_t new_offset)
{
    (void)arena_context;
    if (!header || !header->is_allocated) return 0;

    /* --- CASE 1: Promotion (Generation 3+) --- */
    if (header->custom_flags >= NURSERY_PROMOTION_GENERATION) {
        if(nursury_promotion(header->handle))
            return 0;
        HandleEntry* entry = handle_table_get_entry(header->handle);
            if(!entry)
                return 0;
            entry->data.data_ptr.data_offset = new_offset;
        return 1;
    }
    /* 1. Increment its generation (it survived another cycle!) */
    header->custom_flags++;

    /* 2. Update the Global Handle Table with the NEW offset it is sliding to */
    HandleEntry* entry = handle_table_get_entry(header->handle);
    if(!entry)
        return 0;
    entry->data.data_ptr.data_offset = new_offset;

    /* Return 1 tells bump_defrag to KEEP and SLIDE this block */
    return 1; 
}
int nursery_init(uint32_t initial_size)
{
    if(initialized || initial_size == 0) return 0;
    
    /* Hook up the bump allocator, passing the nursery pointer as context */
    if(bump_init(&nursery.bump, initial_size, nursery_defrag_callback, &nursery))
    {
        initialized = 1;
        return 1;
    }
    return 0;
}

void nursery_destroy()
{
    if(!initialized)
        return;
    bump_destroy(&nursery.bump);
    initialized = 0;
}

uint32_t nursery_malloc(uint32_t size, Handle handle)
{
    nursery_init(NURSERY_START_SIZE);
    if (!initialized || size == 0) return INVALID_DATA_OFFSET;
    
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return INVALID_DATA_OFFSET;
    
    if(size<sizeof(BaseFooter))
        size = sizeof(BaseFooter);
    size+=sizeof(BaseHeader);
    size = ALIGN(size);

    /* Fast Path */
    uint32_t offset = bump_malloc(&nursery.bump, size, handle, 0);

    /* Fallback: Defrag */
    if (offset == INVALID_DATA_OFFSET && (nursery.bump.mem_size - nursery.bump.alloc_memory >= size)) {
        bump_defrag(&nursery.bump);
        offset = bump_malloc(&nursery.bump, size, handle, 0);
    }
    if (offset == INVALID_DATA_OFFSET && bump_grow(&nursery.bump, size)) 
    {
        offset = bump_malloc(&nursery.bump, size, handle, 0);
    }
    if (offset != INVALID_DATA_OFFSET)
    { 
        entry->data.data_ptr.data_offset = offset;
    }
    return offset;
}
void nursery_merge_before(BaseHeader** _header)
{
    if(!_header)return;
    BaseHeader* header = *_header;
    if(!header)return;
    if(header->before_alloc) return;

    BaseFooter prev_size = *((BaseFooter*)header-1);
    BaseHeader* prev_header = (BaseHeader*)((uint8_t*)header-prev_size);
    prev_header->size += header->size;
    if(header->is_allocated)
    {
        prev_header->handle = header->handle;
        HandleEntry* entry = handle_table_get_entry(prev_header->handle);
        entry->data.data_ptr.data_offset = GET_INDEX(prev_header, nursery.bump.mem);

        prev_header->is_allocated = 1;
        prev_header->custom_flags = header->custom_flags;

        nursery.bump.alloc_memory += prev_size;
    }
    *_header = prev_header;
}
void nursery_merge_after(BaseHeader* header)
{
    if(!header)return;
    BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
    if((uint8_t*)next>= (uint8_t*)nursery.bump.mem + nursery.bump.mem_size||next->is_allocated) return;
    if((uint8_t*)next - (uint8_t*)nursery.bump.mem >= nursery.bump.cur_index)
    {
        header->size+=BYTES_TO_HEADER_SIZE(nursery.bump.mem_size-nursery.bump.cur_index);
        if (header->is_allocated) {
            nursery.bump.alloc_memory += nursery.bump.mem_size-nursery.bump.cur_index;
            nursery.bump.cur_index = GET_INDEX(header, nursery.bump.mem)+HEADER_SIZE_TO_BYTES(header->size);
        }
    }
    else{
        header->size += next->size;
        BaseHeader* after_next = (BaseHeader*)((uint8_t*)header + HEADER_SIZE_TO_BYTES(header->size));
        if (header->is_allocated) {
            nursery.bump.alloc_memory += HEADER_SIZE_TO_BYTES(next->size);
            if ((uint8_t*)after_next < (uint8_t*)nursery.bump.mem + nursery.bump.cur_index) {
                after_next->before_alloc = 1;
            }
        }
        else if ((uint8_t*)after_next < (uint8_t*)nursery.bump.mem + nursery.bump.cur_index) {
            after_next->before_alloc = 0;
        }
        
    }
}
void nursery_trim_block(BaseHeader* header, uint32_t new_size)
{
    if(!header || HEADER_SIZE_TO_BYTES(header->size)-new_size < ALIGN(sizeof(BaseHeader)+sizeof(BaseFooter))) return;

    BaseHeader* split_header = (BaseHeader*)((uint8_t*)header+new_size);
    split_header->before_alloc = 1;
    split_header->is_allocated = 1;
    split_header->size = header->size-BYTES_TO_HEADER_SIZE(new_size);
    header->size = BYTES_TO_HEADER_SIZE(new_size);
    nursery_free(GET_INDEX(split_header, nursery.bump.mem));
}
uint32_t nursery_realloc(uint32_t new_size, Handle handle)
{
    nursery_init(NURSERY_START_SIZE);
    if(!initialized)
        return INVALID_DATA_OFFSET;
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return INVALID_DATA_OFFSET;
    uint32_t offset = entry->data.data_ptr.data_offset;
    if(offset == INVALID_DATA_OFFSET)
        return nursery_malloc(new_size, handle);

    BaseHeader* header = (BaseHeader*)(nursery.bump.mem + offset);
    void* ptr = (void*)(header+1);
    if(new_size<sizeof(BaseFooter))
        new_size = sizeof(BaseFooter);
    new_size+=sizeof(BaseHeader);
    new_size = ALIGN(new_size);
    uint32_t before_size = 0, next_size = 0;
    if(new_size <= HEADER_SIZE_TO_BYTES(header->size))
    {
        nursery_trim_block(header,new_size);
        return GET_INDEX(header, nursery.bump.mem);
    }
    BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
    if((uint8_t*)next < nursery.bump.mem + nursery.bump.mem_size && !next->is_allocated)
        next_size = HEADER_SIZE_TO_BYTES(next->size);
    if(HEADER_SIZE_TO_BYTES(header->size)+next_size >= new_size)
    {
        nursery_merge_after(header);
        nursery_trim_block(header,new_size);
        header->custom_flags=0;
        return GET_INDEX(header, nursery.bump.mem);
    }
    uint32_t ptr_size = entry->size;
    if(!header->before_alloc)
    {
        before_size = *((BaseFooter*)header-1);
        if(before_size+HEADER_SIZE_TO_BYTES(header->size) + next_size >= new_size)
        {
            nursery_merge_before(&header);
            nursery_merge_after(header);
            memmove(header+1,ptr,ptr_size);
            nursery_trim_block(header,new_size);
            offset = GET_INDEX(header, nursery.bump.mem);
            entry->data.data_ptr.data_offset = offset;
            header->custom_flags=0;
            return offset;
        }
    }
    uint32_t new_off_set = bump_malloc(&nursery.bump, new_size, handle, 0);
    if (new_off_set == INVALID_DATA_OFFSET && (nursery.bump.mem_size - nursery.bump.alloc_memory >= new_size)) {
        bump_defrag(&nursery.bump);
        if(entry->stratigy_id != ALLOC_TYPE_NURSERY)
            return entry->data.data_ptr.data_offset;
        offset = entry->data.data_ptr.data_offset;
        /* Now try allocating the new space again */
        new_off_set = bump_malloc(&nursery.bump, new_size, handle, 0);
    }
    if (new_off_set == INVALID_DATA_OFFSET) {
        if (bump_grow(&nursery.bump, new_size)) {
            new_off_set = bump_malloc(&nursery.bump, new_size, handle, 0);
        }
    }
    if(new_off_set == INVALID_DATA_OFFSET) return INVALID_DATA_OFFSET;

    ptr = nursery.bump.mem + offset + sizeof(BaseHeader);
    void* new_ptr = nursery.bump.mem + new_off_set + sizeof(BaseHeader);
    memmove(new_ptr, ptr, ptr_size);
    nursery_free(offset);
    BaseHeader* new_header = (BaseHeader*)(nursery.bump.mem + new_off_set);
    new_header->custom_flags = 0;
    entry->data.data_ptr.data_offset = new_off_set;
    return new_off_set;
}

void nursery_free(uint32_t offset)
{
    if (!initialized || offset >= nursery.bump.cur_index) return ;
    
    if(!bump_free(&nursery.bump, offset))
        return;
    BaseHeader* header = (BaseHeader*)(nursery.bump.mem + offset);
    nursery_merge_after(header);
    nursery_merge_before(&header);
    
    uint32_t block_size = HEADER_SIZE_TO_BYTES(header->size);
    offset = GET_INDEX(header, nursery.bump.mem);

    /* The Rollback: If this is the absolute last block, safely pull the frontier back */
    if (offset + block_size >= nursery.bump.cur_index) {
        nursery.bump.cur_index = offset;
        return;
    }

    BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
    if((uint8_t*)next < (uint8_t*)nursery.bump.mem + nursery.bump.mem_size)
    {
        next->before_alloc = 0;
        PUT_FOOTER(header);
    }
}
void* nursery_get(HandleEntry* entry)
{
    if (entry->stratigy_id!=ALLOC_TYPE_NURSERY)
        return NULL;
    return nursery.bump.mem + entry->data.data_ptr.data_offset + sizeof(BaseHeader);
}
Nursery* get_nursery_instance(void)
{
    return &nursery;
}