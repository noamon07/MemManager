#include "Arenas/nursery.h"
#include <sys/mman.h> 
#include <stdint.h>

/* Helper function to get strictly aligned memory from the OS.
 * If block_size is 1MB, this guarantees the address ends in twenty 0s in binary,
 * which is mandatory for the O(1) free() pointer masking trick to work.
 */
static void* mmap_aligned(size_t size, size_t alignment) {
    /* Over-allocate to ensure an aligned chunk exists somewhere inside */
    size_t total_size = size + alignment;
    void* raw = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) return NULL;

    uintptr_t raw_addr = (uintptr_t)raw;
    uintptr_t offset = alignment - (raw_addr % alignment);
    if (offset == alignment) offset = 0; /* Already perfectly aligned */

    void* aligned_addr = (void*)(raw_addr + offset);

    /* Unmap the unused waste memory before and after our aligned block */
    if (offset > 0) {
        munmap(raw, offset);
    }
    size_t tail_size = total_size - (offset + size);
    if (tail_size > 0) {
        munmap((char*)aligned_addr + size, tail_size);
    }

    return aligned_addr;
}

static NurseryNode* create_nursery_node(size_t requested_size) {
    /* 1. Request strictly aligned memory. 
     * Note: requested_size should ideally be a power of 2 (e.g., 1MB, 2MB)
     */
    void* os_memory = mmap_aligned(requested_size, requested_size);
                           
    if (os_memory == NULL) {
        return NULL; 
    }

    /* 2. Place metadata at the perfectly aligned start of the block */
    NurseryNode* node = (NurseryNode*)os_memory;
    
    size_t node_size = sizeof(NurseryNode);
    void* bump_memory = (char*)os_memory + node_size;
    size_t bump_capacity = requested_size - node_size;

    /* NOTE: Standardized to bump_init based on your original code */
    if(!bump_init(bump_memory, bump_capacity))
        return NULL;
    node->allocator = bump_memory;
    
    node->next = NULL;
    node->total_mapped_size = requested_size; 

    return node;
}

void nursery_init(Nursery* nursery, size_t default_block_size) {
    nursery->default_block_size = default_block_size;
    NurseryNode* initial_node = create_nursery_node(default_block_size);
    
    nursery->head = initial_node;
    nursery->active = initial_node;
}

void nursery_destroy(Nursery* nursery) {
    NurseryNode* current = nursery->head;
    
    while (current != NULL) {
        NurseryNode* next_node = current->next;
        size_t size_to_unmap = current->total_mapped_size;
        
        bump_destroy(current->allocator); 
        munmap(current, size_to_unmap);
        
        current = next_node;
    }
    
    nursery->head = NULL;
    nursery->active = NULL;
}

void nursery_reset(Nursery* nursery) {
    NurseryNode* current = nursery->head;
    
    while (current != NULL) {
        bump_reset(current->allocator);
        current = current->next;
    }
    nursery->active = nursery->head;
}

void* nursery_allocate_slow_path(Nursery* nursery, size_t size) {
    NurseryNode* active = nursery->active;

    /* Note: If the user asks for a size larger than the entire block capacity,
     * do NOT put it in the nursery. Route it directly to the TLSF backend instead.
     */
    if (size > nursery->default_block_size - sizeof(NurseryNode)) {
        return NULL; /* Or route to large object allocator */
    }

    /* Scenario 1: Reusing an existing empty block from the chain */
    if (active != NULL && active->next != NULL) {
        nursery->active = active->next;
        void* ptr = bump_alloc(nursery->active->allocator, size);
        if (ptr != NULL) return ptr;
    }

    /* Scenario 2: End of the chain, need to ask OS for a new block */
    NurseryNode* new_node = create_nursery_node(nursery->default_block_size);
    if (new_node == NULL) return NULL; 

    /* Safely append to the end of the chain */
    if (active != NULL) {
        active->next = new_node;
    } else {
        /* Failsafe if the nursery is somehow completely empty */
        nursery->head = new_node;
    }
    
    nursery->active = new_node;
    return bump_alloc(nursery->active->allocator, size);
}

void* nursery_allocate(Nursery* nursery, size_t size) {
    if (nursery->active == NULL) return NULL;

    /* Fast path */
    void* ptr = bump_alloc(nursery->active->allocator, size); // Standardized name
    
    if (ptr != NULL) {
        return ptr; 
    }

    /* Slow path */
    return nursery_allocate_slow_path(nursery, size);
}