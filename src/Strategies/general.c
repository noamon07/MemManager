#include "Strategies/general.h"
#include <string.h>
#include <stdlib.h>
#include "Strategies/strategy.h"


#define GENERAL_START_SIZE (4096)

static General general;
static uint8_t initialized = 0;
static Strategy strategy;

int general_grow(General* allocator, uint32_t requested_size);
int general_init(uint32_t initial_size)
{
    if(initialized || initial_size == 0) return 0;
    if(tlsf_init(&general.tlsf))
    {
        general.mem = (uint8_t*)malloc(initial_size);
        if(!general.mem)
            return 0;
        general.mem_size = initial_size;
        general.alloc_memory = 0;

        BaseHeader* first_block = (BaseHeader*)general.mem;
        first_block->size = BYTES_TO_HEADER_SIZE(initial_size);
        first_block->is_allocated = 0;
        first_block->before_alloc = 1; /* Nothing before it */
        PUT_FOOTER(first_block);

        tlsf_insert(&general.tlsf, general.mem, GET_INDEX(first_block, general.mem));
        general.last_block_allocated = 0;
        strategy.free = general_free;
        strategy.realloc = general_realloc;
        strategy.get = general_get;
        initialized = 1;
        return 1;
    }
    return 0;
}

void general_destroy()
{
    if(!initialized)
        return;
    if(general.mem)
        free(general.mem);
    tlsf_clear(&general.tlsf);
    strategy.free = NULL;
    strategy.realloc = NULL;
    initialized = 0;
}
void general_merge_before(BaseHeader** _header)
{
    if(!_header)return;
    BaseHeader* header = *_header;
    if(!header)return;
    if(header->before_alloc) return;

    BaseFooter prev_size = *((BaseFooter*)header-1);
    BaseHeader* prev_header = (BaseHeader*)((uint8_t*)header-prev_size);

    tlsf_remove(&general.tlsf, general.mem, GET_INDEX(prev_header, general.mem));

    prev_header->size += header->size;
    if(header->is_allocated)
    {
        prev_header->handle = header->handle;
        HandleEntry* entry = handle_table_get_entry(prev_header->handle);
        entry->data.data_ptr.data_offset = GET_INDEX(prev_header, general.mem);

        prev_header->is_allocated = 1;
        prev_header->custom_flags = header->custom_flags;
        general.alloc_memory += prev_size;
    }
    *_header = prev_header;
}
void general_merge_after(BaseHeader* header)
{
    if(!header)return;
    BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
    if((uint8_t*)next>= (uint8_t*)general.mem + general.mem_size)
    {
        general.last_block_allocated = header->is_allocated;
        return;
    }
    if(next->is_allocated) return;

    tlsf_remove(&general.tlsf, general.mem, GET_INDEX(next, general.mem));

    header->size += next->size;
    BaseHeader* after_next = (BaseHeader*)((uint8_t*)header + HEADER_SIZE_TO_BYTES(header->size));
    if (header->is_allocated) {
        if ((uint8_t*)after_next < (uint8_t*)general.mem + general.mem_size) {
            after_next->before_alloc = 1;
        }
        else
        {
            general.last_block_allocated = 1;
        }
        general.alloc_memory += HEADER_SIZE_TO_BYTES(next->size);
    }
    else
    {
        if ((uint8_t*)after_next < (uint8_t*)general.mem + general.mem_size) {
            after_next->before_alloc = 0;
        }
        else
        {
            general.last_block_allocated = 0;
        }
    }
}
void general_trim_block(BaseHeader* header, uint32_t new_size)
{
    if(!header || HEADER_SIZE_TO_BYTES(header->size)-new_size < ALIGN(sizeof(FreeBlockHeader)+sizeof(BaseFooter))) return;

    BaseHeader* split_header = (BaseHeader*)((uint8_t*)header+new_size);
    split_header->before_alloc = 1;
    split_header->is_allocated = 1;
    split_header->size = header->size-BYTES_TO_HEADER_SIZE(new_size);
    header->size = BYTES_TO_HEADER_SIZE(new_size);
    general_free(GET_INDEX(split_header, general.mem));
}

uint32_t general_malloc(uint32_t size, Handle handle)
{
    general_init(GENERAL_START_SIZE);
    if (!initialized || size == 0) return INVALID_DATA_OFFSET;
    
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return INVALID_DATA_OFFSET;
    uint32_t og_size = size;
    size+=sizeof(BaseHeader);
    if(size<sizeof(FreeBlockHeader) + sizeof(BaseFooter))
        size = sizeof(FreeBlockHeader) + sizeof(BaseFooter);
    size = ALIGN(size);

    uint32_t offset = tlsf_find_and_remove(&general.tlsf, general.mem, size);
    if (offset == INVALID_DATA_OFFSET && general_grow(&general, size)) 
    {
        offset = tlsf_find_and_remove(&general.tlsf, general.mem, size);
    }
    if (offset != INVALID_DATA_OFFSET)
    { 
        entry->size = og_size;
        entry->strategy = &strategy;
        BaseHeader* header = (BaseHeader*)(general.mem + offset);
        header->is_allocated = 1;
        header->handle = handle;
        header->custom_flags = 0;
        general.alloc_memory += HEADER_SIZE_TO_BYTES(header->size);
        general_trim_block(header,size);
        BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
        if((uint8_t*)next < general.mem + general.mem_size)
        {
            next->before_alloc = 1;
        }
        else
        {
            general.last_block_allocated = 1;
        }
        entry->data.data_ptr.data_offset = offset;
    }
    return offset;
}

void general_free(uint32_t offset)
{
    if (!initialized || offset >= general.mem_size) return ;
    
    BaseHeader* header = (BaseHeader*)(general.mem + offset);
    header->is_allocated = 0;
    uint32_t og_size = HEADER_SIZE_TO_BYTES(header->size);
    general_merge_after(header);
    general_merge_before(&header);
    offset = GET_INDEX(header, general.mem);
    tlsf_insert(&general.tlsf, general.mem, offset);
    BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
    if((uint8_t*)next < (uint8_t*)general.mem + general.mem_size)
    {
        next->before_alloc = 0;
    }
    PUT_FOOTER(header);
    general.alloc_memory -= og_size;
}

uint32_t general_realloc(uint32_t new_size, Handle handle)
{
    general_init(GENERAL_START_SIZE);
    if(!initialized)
        return INVALID_DATA_OFFSET;
    HandleEntry* entry = handle_table_get_entry(handle);
    if(!entry)
        return INVALID_DATA_OFFSET;
    uint32_t offset = entry->data.data_ptr.data_offset;
    if(offset == INVALID_DATA_OFFSET)
        return general_malloc(new_size, handle);

    BaseHeader* header = (BaseHeader*)(general.mem + offset);
    uint32_t og_new_size = new_size;
    new_size+=sizeof(BaseHeader);
    if(new_size<sizeof(FreeBlockHeader) + sizeof(BaseFooter))
        new_size = sizeof(FreeBlockHeader) + sizeof(BaseFooter);
    new_size = ALIGN(new_size);
    uint32_t before_size = 0, next_size = 0;
    if(new_size <= HEADER_SIZE_TO_BYTES(header->size))
    {
        general_trim_block(header,new_size);
        entry->size = og_new_size;
        return GET_INDEX(header, general.mem);
    }
    BaseHeader* next = (BaseHeader*)((uint8_t*)header+HEADER_SIZE_TO_BYTES(header->size));
    if((uint8_t*)next < general.mem + general.mem_size && !next->is_allocated)
        next_size = HEADER_SIZE_TO_BYTES(next->size);
    if(HEADER_SIZE_TO_BYTES(header->size)+next_size >= new_size)
    {
        general_merge_after(header);
        general_trim_block(header,new_size);
        header->custom_flags=0;
        entry->size = og_new_size;
        return offset;
    }
    uint32_t ptr_size = entry->size;
    void* ptr = (void*)(header+1);
    if(!header->before_alloc)
    {
        before_size = *((BaseFooter*)header-1);
        if(before_size+HEADER_SIZE_TO_BYTES(header->size) + next_size >= new_size)
        {
            general_merge_before(&header);
            general_merge_after(header);
            memmove(header+1,ptr,ptr_size);
            general_trim_block(header,new_size);
            offset = GET_INDEX(header, general.mem);
            entry->data.data_ptr.data_offset = offset;
            header->custom_flags=0;
            entry->size = og_new_size;
            return offset;
        }
    }
    uint32_t new_off_set = general_malloc(new_size, handle);
    if(new_off_set == INVALID_DATA_OFFSET) return INVALID_DATA_OFFSET;

    ptr = general.mem + offset + sizeof(BaseHeader);
    void* new_ptr = general.mem + new_off_set + sizeof(BaseHeader);
    memmove(new_ptr, ptr, ptr_size);
    general_free(offset);
    return new_off_set;

}

void* general_get(uint32_t offset)
{
    return general.mem + offset + sizeof(BaseHeader);
}

General* get_general_instance()
{
    return &general;
}

int general_grow(General* allocator, uint32_t requested_size)
{
    if (!allocator || !allocator->mem || requested_size == 0) return 0;

    uint32_t required = allocator->mem_size + requested_size;
    if (required < allocator->mem_size) return 0; // check if overflow
    uint32_t new_size = allocator->mem_size;
    if (new_size == 0) new_size = 4096;

    while (new_size < required) {
        new_size <<= 1;
        if (new_size == 0) return 0;
    }
    uint8_t* new_mem = (uint8_t*)realloc(general.mem, new_size);
    if (!new_mem) return 0;

    uint32_t old_size = allocator->mem_size;
    general.mem = new_mem;
    general.mem_size = new_size;

    BaseHeader* new_block = (BaseHeader*)(general.mem + old_size);
    new_block->size = BYTES_TO_HEADER_SIZE(new_size - old_size);
    new_block->is_allocated = 0;
    new_block->before_alloc = general.last_block_allocated;
    general.last_block_allocated = 0;
    general_merge_before(&new_block);
    tlsf_insert(&general.tlsf, general.mem, GET_INDEX(new_block, general.mem));
    PUT_FOOTER(new_block);

    return 1;
}