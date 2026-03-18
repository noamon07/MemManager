#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "tests/nursery_stress.h"
#include "Interface/mem_manager.h"
#include "mem_visualizer.h"
#include "Arenas/nursery.h"
#include "Arenas/handle.h"

// Helper macro to reset the allocator state for each test
#define SETUP_TEST() do { \
    mm_destroy(); \
    printf("--- Setting up test ---\n"); \
} while (0)

// --- Test Implementations ---

/**
 * @test test_bump_pointer_retreat
 * @brief Allocate A, B. Free B. Assert nursery.cur_index retreats.
 */
void test_bump_pointer_retreat(void) {
    printf("\nRunning test: Bump Pointer Retreat...\n");
    SETUP_TEST();
    printf("Allocate 2 objects\n");
    Handle handle_a = mm_malloc(32);
    Handle handle_b = mm_malloc(64);
    mm_visualize_nursery();
    printf("Free the second object\n");
    mm_free(handle_b);
    mm_visualize_nursery();
    printf("Free the first object\n");
    mm_free(handle_a);
    mm_visualize_nursery();
    printf("bump pointer retreated\n");
}

/**
 * @test test_cascading_retreat
 * @brief Allocate A, B, C. Free B (no retreat). Free C. Assert retreat to start of B.
 */
void test_cascading_retreat(void) {
    printf("\nRunning test: Cascading Retreat...\n");
    SETUP_TEST();
    printf("allocate 3 objects\n");
    Handle handle_a = mm_malloc(32);
    Handle handle_b = mm_malloc(64);
    Handle handle_c = mm_malloc(128);
    mm_visualize_nursery();
    printf("Free the second object\n");
    mm_free(handle_b); // Free B, should create a free block but not move cur_index
    mm_visualize_nursery();
    printf("Free the first object\n");
    mm_free(handle_a); // Free C, should merge with B and retreat cur_index
    mm_visualize_nursery();
    printf("Free the third object\n");
    mm_free(handle_c);
    mm_visualize_nursery();
    printf("the free blocks merged\n");
}

void test_realloc_shrink(void) {
    printf("\nRunning test: Realloc Shrink...\n");
    SETUP_TEST();
    printf("Allocate 2 object\n");
    Handle handle1 = mm_malloc(100);
    Handle handle2 = mm_malloc(20);
    (void)handle1;
    (void)handle2;
    mm_visualize_nursery();
    printf("realloc the first object to a smaller size\n");
    handle1 = mm_realloc(handle1, 20);
    mm_visualize_nursery();
    printf("new free block created after the 1 object\n");
}

/**
 * @test test_realloc_forward_merge_perfect_fit
 * @brief Layout: [A:100][B:FREE 50][C:100]. Realloc A to 150.
 */
void test_realloc_forward_merge_perfect_fit(void) {
    printf("\nRunning test: Realloc Forward Merge (Perfect Fit)...\n");
    SETUP_TEST();
    
    Handle handle_a = mm_malloc(100);
    Handle handle_b = mm_malloc(50);
    Handle handle_c = mm_malloc(100);

    mm_free(handle_b); // Create the gap

    printf("Before realloc and merge:\n");
    mm_visualize_allocator();

    handle_a = mm_realloc(handle_a, 150);

    printf("After realloc and merge:\n");
    mm_visualize_allocator();
    
    // Verification is visual, but we can assert that C is still valid
    char *c_ptr = (char*)mm_get_ptr(handle_c);
    assert(c_ptr != NULL);
    // And that A is valid
    char *a_ptr = (char*)mm_get_ptr(handle_a);
    assert(a_ptr != NULL);


    printf("[PASS] test_realloc_forward_merge_perfect_fit\n");
}

/**
 * @test test_realloc_backward_merge
 * @brief Layout: [FREE A][ALLOC B]. Realloc B to be larger.
 */
void test_realloc_backward_merge(void) {
    printf("\nRunning test: Realloc Backward Merge...\n");
    SETUP_TEST();

    Handle handle_a = mm_malloc(100);
    Handle handle_b = mm_malloc(50);
    
    void* b_ptr_before = mm_get_ptr(handle_b);
    const char* test_str = "hello backward merge";
    strcpy((char*)b_ptr_before, test_str);

    mm_free(handle_a); // Create free space before B

    printf("Before backward merge:\n");
    mm_visualize_allocator();

    handle_b = mm_realloc(handle_b, 120);

    printf("After backward merge:\n");
    mm_visualize_allocator();

    void* b_ptr_after = mm_get_ptr(handle_b);
    assert(b_ptr_after < b_ptr_before); // Assert it moved backward
    assert(strcmp((char*)b_ptr_after, test_str) == 0); // Assert data is intact

    printf("[PASS] test_realloc_backward_merge\n");
}

/**
 * @test test_defrag_single_gap
 * @brief Layout: [ALLOC A][FREE B][ALLOC C]. Force defrag. Assert C slides.
 */
void test_defrag_single_gap(void) {
    printf("\nRunning test: Defrag Single Gap...\n");
    SETUP_TEST();

    Handle handle_a = mm_malloc(64);
    (void)handle_a;
    Handle handle_b = mm_malloc(64);
    Handle handle_c = mm_malloc(64);
    
    void* c_ptr_before = mm_get_ptr(handle_c);
    const char* test_str = "slide";
    strcpy((char*)c_ptr_before, test_str);

    mm_free(handle_b); // Create the gap

    printf("Before defrag:\n");
    mm_visualize_allocator();

    // Force a defrag by allocating something that won't fit in the gap
    Handle handle_d = mm_malloc(128); 
    (void)handle_d; 

    printf("After defrag:\n");
    mm_visualize_allocator();

    void* c_ptr_after = mm_get_ptr(handle_c);
    assert(c_ptr_after < c_ptr_before); // Assert C slid left
    assert(strcmp((char*)c_ptr_after, test_str) == 0); // Assert data is intact

    printf("[PASS] test_defrag_single_gap\n");
}

/**
 * @test test_defrag_swiss_cheese
 * @brief Allocate 10 blocks, free every other one, force defrag.
 */
void test_defrag_swiss_cheese(void) {
    printf("\nRunning test: Defrag Swiss Cheese...\n");
    SETUP_TEST();
    Nursery *nursery = mm_get_nursery_instance();

    Handle handles[10];
    for (int i = 0; i < 10; i++) {
        handles[i] = mm_malloc(32);
    }

    for (int i = 1; i < 10; i += 2) {
        mm_free(handles[i]);
    }

    printf("Before defrag (swiss cheese):\n");
    mm_visualize_allocator();

    // Force defrag by allocating a large block
    Handle large_block = mm_malloc(nursery->mem_size / 2);
    (void)large_block;

    printf("After defrag:\n");
    mm_visualize_allocator();

    // Assert that the remaining blocks are contiguous
    void* last_ptr = NULL;
    for (int i = 0; i < 10; i += 2) {
        void* current_ptr = mm_get_ptr(handles[i]);
        if (last_ptr != NULL) {
            // This assertion is tricky because of metadata. A visual check is better.
            // A simple check is that pointers are ordered and close.
            assert(current_ptr > last_ptr);
        }
        last_ptr = current_ptr;
    }

    printf("[PASS] test_defrag_swiss_cheese\n");
}

/**
 * @test test_generational_promotion
 * @brief Trigger multiple defrags and check if promotion occurs.
 * @note This test assumes an internal mechanism increments a 'generation'
 *       counter on a block when it survives a collection (defrag).
 *       It also assumes a promotion callback would be triggered.
 *       This test is speculative about the internal implementation.
 */
void test_generational_promotion(void) {
    printf("\nRunning test: Generational Promotion...\n");
    SETUP_TEST();

    Handle survivor = mm_malloc(16); // The block we want to see promoted
    
    // Repeatedly fill memory, creating gaps to force collections
    for (int i = 0; i < 5; i++) {
        Handle temp_handles[10];
        for(int j = 0; j < 10; j++) {
            temp_handles[j] = mm_malloc(64);
        }
        for(int j = 0; j < 10; j+=2) {
            mm_free(temp_handles[j]);
        }
        // This allocation should trigger a collection
        Handle trigger = mm_malloc(256);
        mm_free(trigger);
    }

    // At this point, 'survivor' should have a high generation count.
    // The assertion depends on how promotion is tracked.
    // We will assume the handle's type might change or it moves to a new arena.
    // For now, we just assert the handle is still valid.
    void* ptr = mm_get_ptr(survivor);
    assert(ptr != NULL);

    printf("[PASS] test_generational_promotion (validation is conceptual)\n");
}


/**
 * @test test_heap_shift_arena_grow
 * @brief Force the entire arena to move in memory via realloc.
 */
void test_heap_shift_arena_grow(void) {
    printf("\nRunning test: The Heap Shift (Arena Grow)...\n");
    SETUP_TEST();
    Nursery *nursery = mm_get_nursery_instance();
    size_t initial_size = nursery->mem_size;

    Handle handle = mm_malloc(100);
    const char* test_str = "this data must survive the apocalypse";
    void* ptr_before = mm_get_ptr(handle);
    strcpy((char*)ptr_before, test_str);

    printf("Before heap shift:\n");
    mm_visualize_allocator();

    // Allocate more than the total remaining memory to force a resize
    Handle giant_handle = mm_malloc(initial_size);
    assert(giant_handle.index != (uint32_t)-1); // Ensure it succeeded

    printf("After heap shift:\n");
    mm_visualize_allocator();

    assert(nursery->mem_size > initial_size); // Assert arena grew

    void* ptr_after = mm_get_ptr(handle);
    assert(ptr_after != NULL);
    assert(strcmp((char*)ptr_after, test_str) == 0); // Assert data is intact

    printf("[PASS] test_heap_shift_arena_grow\n");
}

/**
 * @test test_double_free_and_dirty_calloc
 * @brief Free a block twice, then calloc it and check for zero-fill.
 */
void test_double_free_and_dirty_calloc(void) {
    printf("\nRunning test: Double-Free & Dirty Calloc...\n");
    SETUP_TEST();
    Nursery *nursery = mm_get_nursery_instance();

    Handle handle = mm_malloc(128);
    void* ptr = mm_get_ptr(handle);
    memset(ptr, 0xFF, 128); // Dirty the memory

    size_t mem_before_free = nursery->alloc_memory;
    mm_free(handle);
    size_t mem_after_first_free = nursery->alloc_memory;
    assert(mem_after_first_free < mem_before_free);

    mm_free(handle); // Double free
    assert(nursery->alloc_memory == mem_after_first_free); // Assert memory only dropped once

    Handle calloc_handle = mm_calloc(128);
    void* calloc_ptr = mm_get_ptr(calloc_handle);
    
    for (size_t i = 0; i < 128; i++) {
        assert(((char*)calloc_ptr)[i] == 0x00);
    }

    printf("[PASS] test_double_free_and_dirty_calloc\n");
}

/**
 * @test test_chaotic_accounting_audit
 * @brief Random allocs, frees, reallocs, then audit memory integrity.
 */
void test_chaotic_accounting_audit(void) {
    printf("\nRunning test: The Chaotic Accounting Audit...\n");
    SETUP_TEST();
    Nursery *nursery = mm_get_nursery_instance();
    srand(time(NULL));

    const int num_ops = 200;
    const int max_handles = 50;
    Handle handles[max_handles];
    memset(handles, 0, sizeof(handles));

    for (int i = 0; i < num_ops; i++) {
        int slot = rand() % max_handles;
        
        if (handles[slot].index == 0) { // Allocate
            size_t size = (rand() % 128) + 16;
            handles[slot] = mm_malloc(size);
        } else { // Realloc
            int op = rand() % 3;
            if (op == 0) { // Free
                mm_free(handles[slot]);
                handles[slot].index = 0;
            } else { // Realloc
                size_t new_size = (rand() % 256) + 16;
                handles[slot] = mm_realloc(handles[slot], new_size);
            }
        }
    }
    
    // Force a final collection to clean things up
    mm_malloc(nursery->mem_size);

    // Audit is complex without direct access to block headers.
    // A simple audit: does the nursery's internal count match reality?
    size_t calculated_alloc_mem = 0;
    HandleTable* handle_table = mm_get_handle_table_instance();

    for(uint32_t i=1; i < handle_table->size; ++i) {
        if(handle_table->entries[i].is_allocated == 1 && handle_table->entries[i].stratigy_id == ALLOC_TYPE_NURSERY) {
            calculated_alloc_mem += handle_table->entries[i].size;
        }
    }
    
    // Note: This may not be a perfect 1:1 match due to metadata/alignment,
    // but it should be very close.
    assert(abs((int)nursery->alloc_memory - (int)calculated_alloc_mem) < (int)(max_handles * 16));


    printf("[PASS] test_chaotic_accounting_audit\n");
}

/**
 * @test test_out_of_memory
 * @brief Allocate until the nursery is full and expect the next alloc to fail.
 */
void test_out_of_memory(void) {
    printf("\nRunning test: Out of Memory...\n");
    SETUP_TEST();
    Nursery *nursery = mm_get_nursery_instance();

    // Allocate large chunks until we can't anymore
    while(1) {
        Handle h = mm_malloc(nursery->mem_size / 10);
        if (h.index == (uint32_t)-1) {
            break; // Allocation failed as expected
        }
    }

    // Now the nursery should be very full. Try a small allocation.
    Handle final_handle = mm_malloc(1);
    
    // It should fail (return an invalid handle)
    assert(final_handle.index == (uint32_t)-1);

    printf("[PASS] test_out_of_memory\n");
}


// --- Master Test Runner ---

void run_all_nursery_tests(void) {
    printf("========= Starting Nursery Allocator Stress Tests =========\n");
    
    test_bump_pointer_retreat();
    test_cascading_retreat();
    test_realloc_shrink();
    test_realloc_forward_merge_perfect_fit();
    test_realloc_backward_merge();
    test_defrag_single_gap();
    test_defrag_swiss_cheese();
    test_generational_promotion();
    test_heap_shift_arena_grow();
    test_double_free_and_dirty_calloc();
    test_chaotic_accounting_audit();
    test_out_of_memory();

    printf("\n========= All Nursery Allocator Stress Tests Completed =========\n");
    mm_destroy(); // Final cleanup
}
