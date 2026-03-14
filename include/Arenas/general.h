#ifndef GENERAL_ALLOCATOR_H
#define GENERAL_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

/* --- Configuration Macros --- */

// TLSF Configuration
#define FLI_MAX 32        // First-Level Index count (e.g., up to 4GB sizes)
#define SLI_LOG2 4        // Second-Level Index log2 (e.g., 4 means 16 subdivisions per FLI)
#define SLI_COUNT (1 << SLI_LOG2)

// Slab Configuration
#define MAX_DYNAMIC_SLABS 16 // Maximum number of highly-frequent sizes to track
#define SLAB_PAGE_SIZE 4096  // Size of the chunk requested from TLSF to back a slab

/* --- Data Structures --- */

// Forward declaration of the main allocator state
typedef struct allocator_state allocator_t;

/* * TLSF Block Header
 * Represents a chunk of memory managed by the TLSF backend.
 */
typedef struct tlsf_block {
    size_t size;                    // Size of the block (including flags in lowest bits)
    struct tlsf_block* prev_phys;   // Pointer to the physically adjacent previous block (for coalescing)
    struct tlsf_block* next_free;   // Next block in the segregated free list
    struct tlsf_block* prev_free;   // Previous block in the segregated free list
} tlsf_block_t;

/* * Slab Header
 * Manages a chunk of memory divided into identical, fixed-size slots.
 */
typedef struct slab {
    size_t slot_size;               // Size of each allocation in this slab
    uint32_t total_slots;           // Total capacity of the slab
    uint32_t free_slots;            // Number of currently available slots
    void* free_list;                // Simple linked list of free slots within this slab
    struct slab* next_slab;         // Pointer to next slab of the same slot size
} slab_t;

/* * Slab Manager
 * Tracks frequency of allocations to dynamically promote sizes into slabs.
 */
typedef struct {
    size_t slot_size;       // The specific size this class handles
    slab_t* active_slabs;   // Linked list of slabs supplying this size
    uint32_t request_count; // Heuristic counter to determine if this size is "frequent"
} slab_class_t;

/* * Main Allocator State
 * Holds the bitmaps for TLSF and the array of active dynamic slabs.
 */
struct allocator_state {
    // TLSF Backend State
    uint32_t fl_bitmap;                           // First-Level bitmap
    uint32_t sl_bitmap[FLI_MAX];                  // Second-Level bitmaps
    tlsf_block_t* free_lists[FLI_MAX][SLI_COUNT]; // The segregated free lists
    
    // Dynamic Slab Frontend State
    slab_class_t slab_classes[MAX_DYNAMIC_SLABS]; // Tracked frequent sizes
    size_t active_slab_classes;                   // How many slab sizes are currently active
    
    // Memory pool tracking
    void* initial_pool;
    size_t pool_capacity;
};

/* --- API Functions --- */

/**
 * Initializes the allocator with a raw block of memory.
 * This memory will be formatted into the initial TLSF block.
 */
allocator_t* allocator_init(void* memory_pool, size_t pool_size);

/**
 * Allocates 'size' bytes. 
 * Checks the dynamic slabs first. If no slab exists for this size, it either:
 * 1. Creates a slab (if the size is requested frequently enough).
 * 2. Falls back to the TLSF backend for a direct allocation.
 */
void* allocator_malloc(allocator_t* alloc, size_t size);

/**
 * Frees the memory pointed to by 'ptr'.
 * Determines if 'ptr' belongs to a slab or the TLSF backend, and routes
 * the free operation to the appropriate subsystem.
 */
void allocator_free(allocator_t* alloc, void* ptr);

/**
 * Resizes the memory block pointed to by 'ptr'.
 */
void* allocator_realloc(allocator_t* alloc, void* ptr, size_t new_size);

/**
 * Destroys the allocator structure (useful for testing/cleanup).
 */
void allocator_destroy(allocator_t* alloc);

/* --- Debugging & Heuristics (Optional) --- */

/**
 * Prints the current state of fragmentation, active slabs, and TLSF lists.
 */
void allocator_dump_stats(allocator_t* alloc);

#endif // GENERAL_ALLOCATOR_H