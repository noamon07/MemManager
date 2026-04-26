#ifndef MM_API_H
#define MM_API_H
#include <stdint.h>
#include <stddef.h>
#define DEFAULT_MM_CONFIG(total_mem) (MemoryManagerConfig){ \
    .total_system_memory = (total_mem), \
    .max_handles = 1024, \
    .nursery_ratio = 0.20f, \
    .expected_graph_density = 2.0f \
}
typedef struct {
    uint32_t index:24,
             generation:8;
} Handle;
typedef struct {
    size_t total_system_memory;
    uint32_t max_handles;
    float nursery_ratio;       // e.g., 0.20 for 20%
    float expected_graph_density; // Average edges per handle (e.g., 2.5)
} MemoryManagerConfig;
#define INVALID_INDEX ((1U << 24) - 1)
#define INVALID_HANDLE ((Handle){INVALID_INDEX, 0})

int mm_init(MemoryManagerConfig config);
void mm_destroy();
void* mm_get_ptr(Handle handle);
Handle mm_malloc(size_t size);
void mm_free(Handle handle);
Handle mm_realloc(Handle handle, size_t new_size);
Handle mm_calloc(size_t size);
void mem_set_ref(Handle parent, Handle child);
void mem_remove_ref(Handle parent, Handle child);
int handles_equal(Handle a, Handle b);
int is_valid_handle(Handle h);
void write_handle(Handle handle);
void clear_handle(Handle handle);
#endif