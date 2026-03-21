#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "Interface/mem_manager.h"
#include "Arenas/handle.h"
#include "Strategies/nursery.h"
#include "mem_visualizer.h"

/* --- Test Helper Macros --- */
#define TEST_START(name) printf("[TEST] %s...\n", name)
#define TEST_PASS()      printf("  -> PASSED\n\n")

/* Internal State Access */
Nursery* get_nursery() { return get_nursery_instance(); }

/* Helper to get physical BaseHeader from a Handle */
BaseHeader* get_header(Handle h) {
    HandleEntry* entry = handle_table_get_entry(h);
    if (!entry) return NULL;
    return (BaseHeader*)(get_nursery()->bump.mem + entry->data.data_ptr.data_offset);
}

/* ========================================================================= */
/* 1. Frontier Rollback                                                      */
/* ========================================================================= */
void test_1_frontier_rollback() {
    TEST_START("1. Frontier Rollback");
    Nursery* n = get_nursery();
    uint32_t start_index = n->bump.cur_index;

    Handle h1 = mm_malloc(64);
    assert(h1.index != INVALID_INDEX);
    
    mm_free(h1);
    
    // The frontier must instantly snap back, leaving no ghost block
    assert(n->bump.cur_index == start_index);
    assert(n->bump.alloc_memory == 0);
    TEST_PASS();
}

/* ========================================================================= */
/* 2. O(1) Bidirectional Coalescing                                          */
/* ========================================================================= */
void test_2_bidirectional_coalesce() {
    TEST_START("2. O(1) Bidirectional Coalescing");
    Nursery* n = get_nursery();

    Handle h1 = mm_malloc(32);
    Handle h2 = mm_malloc(32); // Free 1
    Handle h3 = mm_malloc(32); // Center block
    Handle h4 = mm_malloc(32); // Free 2
    Handle h5 = mm_malloc(32); // Lock frontier

    uint32_t offset_h2 = handle_table_get_entry(h2)->data.data_ptr.data_offset;
    uint32_t size2 = HEADER_SIZE_TO_BYTES(get_header(h2)->size);
    uint32_t size3 = HEADER_SIZE_TO_BYTES(get_header(h3)->size);
    uint32_t size4 = HEADER_SIZE_TO_BYTES(get_header(h4)->size);

    mm_free(h2);
    mm_free(h4);
    
    // Freeing the center block must trigger bidirectional coalesce
    mm_free(h3);

    // Verify the giant hole exists and starts exactly at h2's old offset
    BaseHeader* merged_hole = (BaseHeader*)(n->bump.mem + offset_h2);
    assert(merged_hole->is_allocated == 0);
    assert(HEADER_SIZE_TO_BYTES(merged_hole->size) == (size2 + size3 + size4));

    // Verify Boundary Tag (Footer) at the end of the giant hole
    BaseFooter* footer = (BaseFooter*)((uint8_t*)merged_hole + HEADER_SIZE_TO_BYTES(merged_hole->size) - sizeof(BaseFooter));
    assert(*footer == (size2 + size3 + size4));

    mm_free(h5);
    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 3. Reallocation: In-Place Shrink                                          */
/* ========================================================================= */
void test_3_inplace_shrink() {
    TEST_START("3. Reallocation: In-Place Shrink");
    Nursery* n = get_nursery();

    Handle h1 = mm_malloc(128);
    Handle hLock = mm_malloc(32); // Lock it in

    uint32_t original_offset = handle_table_get_entry(h1)->data.data_ptr.data_offset;
    uint32_t starting_alloc = n->bump.alloc_memory;

    // Shrink the block
    mm_realloc(h1, 32);

    // Offset must not change (in-place)
    assert(handle_table_get_entry(h1)->data.data_ptr.data_offset == original_offset);
    
    // Alloc memory MUST decrease because the scrap was freed
    assert(n->bump.alloc_memory < starting_alloc);

    mm_free(hLock);
    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 4. Reallocation: Frontier Expansion                                       */
/* ========================================================================= */
void test_4_frontier_expansion() {
    TEST_START("4. Reallocation: Frontier Expansion");
    Nursery* n = get_nursery();

    Handle h1 = mm_malloc(32);
    uint32_t original_offset = handle_table_get_entry(h1)->data.data_ptr.data_offset;
    
    // Because h1 is the last block, it should expand forward effortlessly
    mm_realloc(h1, 256);

    // Offset must NOT change, meaning no memmove was performed
    assert(handle_table_get_entry(h1)->data.data_ptr.data_offset == original_offset);
    
    // Cur index should have advanced to cover the new size
    BaseHeader* header = get_header(h1);
    assert(n->bump.cur_index == original_offset + HEADER_SIZE_TO_BYTES(header->size));

    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 5. Reallocation: Neighbor Absorption                                      */
/* ========================================================================= */
void test_5_neighbor_absorption() {
    TEST_START("5. Reallocation: Neighbor Absorption");
    
    Handle h1 = mm_malloc(32);
    Handle h2 = mm_malloc(128); // The neighbor we will free
    Handle hLock = mm_malloc(32); 

    uint32_t original_offset = handle_table_get_entry(h1)->data.data_ptr.data_offset;
    mm_free(h2); // Create a 128-byte hole

    // h1 only needs 64 bytes total, the 128 byte hole is right next to it
    mm_realloc(h1, 64);

    // Offset must not change, it just ate the hole
    assert(handle_table_get_entry(h1)->data.data_ptr.data_offset == original_offset);

    mm_free(hLock);
    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 6. Reallocation: Full Relocation & Handle Update                          */
/* ========================================================================= */
void test_6_full_relocation() {
    TEST_START("6. Reallocation: Full Relocation & Handle Update");
    
    Handle h1 = mm_malloc(32);
    Handle h2 = mm_malloc(32); // Locks h1 completely
    
    uint32_t old_offset = handle_table_get_entry(h1)->data.data_ptr.data_offset;

    // Needs to grow, but is boxed in. Must relocate to frontier.
    mm_realloc(h1, 256);

    uint32_t new_offset = handle_table_get_entry(h1)->data.data_ptr.data_offset;
    
    // The handle MUST have been updated to a new location
    assert(new_offset != old_offset);
    assert(new_offset > old_offset);

    mm_free(h1);
    mm_free(h2);
    TEST_PASS();
}

/* ========================================================================= */
/* 7. Sliding Compaction (Defragmentation)                                   */
/* ========================================================================= */
void test_7_sliding_compaction() {
    TEST_START("7. Sliding Compaction (Defragmentation)");
    Nursery* n = get_nursery();

    Handle h1 = mm_malloc(32);
    Handle h2 = mm_malloc(32); // Free
    Handle h3 = mm_malloc(32); // Slide target
    Handle h4 = mm_malloc(32); // Free
    Handle h5 = mm_malloc(32); // Slide target

    mm_free(h2);
    mm_free(h4);

    // Manually trigger the slide
    bump_defrag(&n->bump);

    BaseHeader* head1 = get_header(h1);
    uint32_t off1 = handle_table_get_entry(h1)->data.data_ptr.data_offset;
    uint32_t off3 = handle_table_get_entry(h3)->data.data_ptr.data_offset;
    
    // h3 must now physically sit perfectly adjacent to h1
    assert(off3 == off1 + HEADER_SIZE_TO_BYTES(head1->size));

    mm_free(h1);
    mm_free(h3);
    mm_free(h5);
    TEST_PASS();
}

/* ========================================================================= */
/* 8. Generational Aging & Promotion Routing                                 */
/* ========================================================================= */
void test_8_generational_aging() {
    TEST_START("8. Generational Aging & Promotion Routing");
    Nursery* n = get_nursery();

    Handle h1 = mm_malloc(32);
    
    assert(get_header(h1)->custom_flags == 0);

    // Defrag 3 times to age the block
    bump_defrag(&n->bump);
    assert(get_header(h1)->custom_flags == 1);
    
    bump_defrag(&n->bump);
    assert(get_header(h1)->custom_flags == 2);
    
    bump_defrag(&n->bump);
    assert(get_header(h1)->custom_flags == 3);

    bump_defrag(&n->bump);
    // On the 4th defrag > (NURSERY_PROMOTION_GENERATION), the callback returns 0 
    // because nursury_promotion drops it (currently mocked). 
    // Thus, it should be erased from the Nursery footprint!
    assert(n->bump.alloc_memory == 0);
    assert(n->bump.cur_index == 0);

    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 9. Dynamic Arena Growth                                                   */
/* ========================================================================= */
void test_9_dynamic_growth() {
    TEST_START("9. Dynamic Arena Growth");
    Nursery* n = get_nursery();
    
    uint32_t starting_mem_size = n->bump.mem_size;
    if (starting_mem_size == 0) starting_mem_size = 4096;

    // Ask for memory larger than the initial pool
    Handle h1 = mm_malloc(starting_mem_size + 1024);
    
    assert(h1.index != INVALID_INDEX);
    assert(n->bump.mem_size > starting_mem_size); // It successfully grew!

    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 10. Byte Alignment & Padding                                              */
/* ========================================================================= */
void test_10_byte_alignment() {
    TEST_START("10. Byte Alignment & Padding");

    // Requesting exactly 3 bytes
    Handle h1 = mm_malloc(3);
    BaseHeader* header = get_header(h1);
    
    uint32_t total_bytes = HEADER_SIZE_TO_BYTES(header->size);
    
    // Header (8) + Payload (3) = 11.
    // Alignment is 8 bytes, so 11 aligns up to 16 bytes.
    assert(total_bytes == 16);
    assert(total_bytes % 8 == 0);

    mm_free(h1);
    TEST_PASS();
}

/* ========================================================================= */
/* 11. OS Denial / OOM Fallback                                              */
/* ========================================================================= */
void test_11_oom_fallback() {
    TEST_START("11. OS Denial / OOM Fallback");
    
    // Request an absurdly large allocation that will cause bump_grow math to fail or OS to reject
    // ~4 Gigabytes
    Handle h1 = mm_malloc(0xFFFFFF00); 
    
    // The allocator must gracefully fail and bubble up INVALID_INDEX
    assert(h1.index == INVALID_INDEX);

    TEST_PASS();
}

/* ========================================================================= */
/* 12. High-Churn Stress Test                                                */
/* ========================================================================= */
void test_12_high_churn_stress() {
    TEST_START("12. High-Churn Stress Test");
    Nursery* n = get_nursery();
    
    #define CHURN_COUNT 1000
    Handle active_handles[CHURN_COUNT];
    for (int i = 0; i < CHURN_COUNT; i++) {
        active_handles[i].index = INVALID_INDEX;
        active_handles[i].generation = 0;
    }

    // Seed randomness for reproducibility
    srand(42);

    for (int i = 0; i < 5000; i++) {
        int slot = rand() % CHURN_COUNT;
        int action = rand() % 3;
        if (action == 0 || active_handles[slot].index == INVALID_INDEX) {
            // ALLOCATE
            if (active_handles[slot].index != INVALID_INDEX) {
                mm_free(active_handles[slot]);
            }
            uint32_t size = (rand() % 256) + 1;
            active_handles[slot] = mm_malloc(size);
            
        } else if (action == 1) {
            // FREE
            mm_free(active_handles[slot]);
            active_handles[slot].index = INVALID_INDEX;
            
        } else if (action == 2) {
            // REALLOC
            uint32_t new_size = (rand() % 512) + 1;
            active_handles[slot] = mm_realloc(active_handles[slot], new_size);
        }
    }

    // Force one final defrag to compact the resulting Swiss cheese
    bump_defrag(&n->bump);

    // Validate structural integrity: Walk memory and sum up live blocks
    uint32_t calculated_alloc = 0;
    uint32_t offset = 0;
    
    while (offset < n->bump.cur_index) {
        BaseHeader* head = (BaseHeader*)(n->bump.mem + offset);
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(head->size);
        
        if (head->is_allocated) {
            calculated_alloc += block_bytes;
        } else {
            // If we defragged, there should literally be ZERO free blocks left behind the frontier.
            assert(0 && "Defrag failed to squash a hole!");
        }
        offset += block_bytes;
    }

    // Tracker perfectly matches the physical memory walk
    assert(calculated_alloc == n->bump.alloc_memory);

    // Clean up
    for (int i = 0; i < CHURN_COUNT; i++) {
        if (active_handles[i].index != INVALID_INDEX) {
            mm_free(active_handles[i]);
        }
    }
    // Total rollback to 0 proves the system is entirely watertight
    assert(n->bump.cur_index == 0);
    assert(n->bump.alloc_memory == 0);

    TEST_PASS();
}

/* ========================================================================= */
void run_all_tests() {
    printf("====================================================\n");
    printf("   GENERATIONAL HYBRID MEMORY MANAGER TEST SUITE    \n");
    printf("====================================================\n\n");

    test_1_frontier_rollback();
    test_2_bidirectional_coalesce();
    test_3_inplace_shrink();
    test_4_frontier_expansion();
    test_5_neighbor_absorption();
    test_6_full_relocation();
    test_7_sliding_compaction();
    test_8_generational_aging();
    test_9_dynamic_growth();
    test_10_byte_alignment();
    test_11_oom_fallback();
    test_12_high_churn_stress();

    printf("====================================================\n");
    printf("         ALL 12 RIGOROUS TESTS PASSED!              \n");
    printf("====================================================\n");

}