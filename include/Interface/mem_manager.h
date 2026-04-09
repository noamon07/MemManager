#ifndef MM_API_H
#define MM_API_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t index:24,
             generation:8;
} Handle;
typedef struct {
    Handle handle;
    uint32_t byte_offset;
} Cursor;

#define INVALID_INDEX ((1U << 24) - 1)
#define INVALID_HANDLE ((Handle){INVALID_INDEX, 0})
#define MEM_CURSOR_WRITE(cur, value) do { \
    if (__builtin_types_compatible_p(__typeof__(value), Handle)) { \
        _internal_write_handle((cur), (void*)&(value)); \
    } else { \
        _internal_write_data((cur), (void*)&(value), sizeof(value)); \
    } \
} while(0)
int mm_init(size_t max_size);
void mm_destroy();
void* mm_get_ptr(Handle handle);
Handle mm_malloc(size_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, size_t new_size);
Handle mm_calloc(size_t size);
void mem_set_ref(Handle parent, Handle child);
void mem_remove_ref(Handle parent, Handle child);
Cursor mem_get_cursor(Handle handle);
void _internal_write_data(Cursor cur, void* ptr, uint32_t size);
void _internal_write_handle(Cursor cur, void* ptr);
int handles_equal(Handle a, Handle b);
int is_valid_handle(Handle h);
#endif