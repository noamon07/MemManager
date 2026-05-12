#ifndef MM_API_H
#define MM_API_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Helper macro to generate a balanced default configuration.
 * @param total_mem Total heap size in bytes.
 */
#define DEFAULT_MM_CONFIG(total_mem) (MemoryManagerConfig){ \
    .total_system_memory = (total_mem), \
    .max_handles = 1024, \
    .nursery_ratio = 0.20f, \
    .expected_graph_density = 2.0f \
}

/**
 * @struct Handle
 * @brief A generation-aware opaque reference to a memory block.
 * Uses 24 bits for the table index and 8 bits for the generation counter.
 */
typedef struct {
    uint32_t index:24,
             generation:8;
} Handle;

/**
 * @struct MemoryManagerConfig
 * @brief Startup parameters for the memory manager.
 */
typedef struct {
    size_t total_system_memory;    /* Total bytes available for the heap */
    uint32_t max_handles;          /* Size of the indirection table */
    float nursery_ratio;           /* Portion of memory reserved for the Nursery (0.0 - 1.0) */
    float expected_graph_density;  /* Estimated edges per object (optimizes Edge Slab size) */
} MemoryManagerConfig;

/** Sentinel values for uninitialized or null handles. */
#define INVALID_INDEX ((1U << 24) - 1)
#define INVALID_HANDLE ((Handle){INVALID_INDEX, 0})

/* --- System Lifecycle --- */

/**
 * @brief Initializes the memory manager, arenas, and handle tables.
 * @return 1 on success, 0 on failure.
 */
int mm_init(MemoryManagerConfig config);

/**
 * @brief Shuts down the system and releases all OS-level memory.
 */
void mm_destroy();

/* --- Core Allocation --- */

/**
 * @brief Resolves a Handle to a raw pointer.
 * @warning Do not store this pointer; it may become invalid after GC or defragmentation.
 */
void* mm_get_ptr(Handle handle);

/**
 * @brief Allocates a managed block of memory.
 * @return A unique Handle to the allocated memory.
 */
Handle mm_malloc(size_t size);

/**
 * @brief Explicitly frees a managed block.
 */
void mm_free(Handle handle);

/**
 * @brief Resizes an existing managed block.
 */
Handle mm_realloc(Handle handle, size_t new_size);

/**
 * @brief Allocates and zero-initializes a managed block.
 */
Handle mm_calloc(size_t size);

/* --- Reference Tracking (GC Input) --- */

/**
 * @brief Registers a pointer reference from 'parent' to 'child'.
 * This is essential for the GC to know that the child is still reachable.
 */
void mem_set_ref(Handle parent, Handle child);

/**
 * @brief Removes a previously registered reference.
 */
void mem_remove_ref(Handle parent, Handle child);

/* --- Utility Functions --- */

/** @brief Checks if two handles point to the same logical object. */
int handles_equal(Handle a, Handle b);

/** @brief Validates if a handle is currently active and not stale. */
int is_valid_handle(Handle h);

/** @brief Pins a handle as a 'Root' to prevent it from being GC'd. */
void write_handle(Handle handle);

/** @brief Unpins a handle, allowing the GC to reclaim it if unreachable. */
void clear_handle(Handle handle);

#endif /* MM_API_H */