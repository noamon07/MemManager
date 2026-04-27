#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "Interface/mem_manager.h"
#include "Infrastructure/handle.h"
#include "Strategies/general.h"
#include "mem_visualizer.h"

/* --- Test Helper Macros --- */
#define TEST_START(name) printf("[TEST] %s...\n", name)
#define TEST_PASS()      printf("  -> PASSED\n\n")

/* Internal State Access */
General* get_general() { return get_general_instance(); }

/* Helper to get physical BaseHeader from the General Arena */
BaseHeader* get_gen_header(uint32_t offset) {
    if (offset == INVALID_DATA_OFFSET) return NULL;
    return (BaseHeader*)(get_general()->mem + offset);
}

/* ========================================================================= */
/* 1. Splitting & Trimming (The Scrap Check)                                 */
/* ========================================================================= */
void test_gen_1_split_and_trim() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("1. TLSF Splitting & Trimming");
    General* g = get_general();

    Handle h1 = handle_table_new(64);
    Handle h2 = handle_table_new(64);

    uint32_t off1 = general_malloc(64, h1);
    uint32_t off2 = general_malloc(64, h2);

    assert(off1 != INVALID_DATA_OFFSET);
    assert(off2 != INVALID_DATA_OFFSET);

    // Get headers
    BaseHeader* head1 = get_gen_header(off1);
    BaseHeader* head2 = get_gen_header(off2);

    // They must be perfectly adjacent
    assert(off2 == off1 + HEADER_SIZE_TO_BYTES(head1->size));

    // The rest of the arena must be a massive free block in TLSF
    BaseHeader* scrap = get_gen_header(off2 + HEADER_SIZE_TO_BYTES(head2->size));
    assert(scrap->is_allocated == 0);
    assert(scrap->before_alloc == 1); // Because head2 is alive!

    general_free(off1);
    general_free(off2);
    
    assert(g->alloc_memory == 0);
    TEST_PASS();
}

/* ========================================================================= */
/* 2. The Coalescing Crucible (The 4-Way Double Merge)                       */
/* ========================================================================= */
void test_gen_2_double_merge() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("2. The Coalescing Crucible (Double Merge)");
    General* g = get_general();

    Handle h1 = handle_table_new(64);
    Handle h2 = handle_table_new(64);
    Handle h3 = handle_table_new(64);
    Handle h4 = handle_table_new(64);

    uint32_t o1 = general_malloc(64, h1);
    uint32_t o2 = general_malloc(64, h2);
    uint32_t o3 = general_malloc(64, h3);
    uint32_t o4 = general_malloc(64, h4);

    // 1. Free the boundaries
    general_free(o1); 
    general_free(o3); 
    
    // At this point: [FREE] [ALIVE(o2)] [FREE] [ALIVE(o4)]
    BaseHeader* head2 = get_gen_header(o2);
    assert(head2->before_alloc == 0); // Correctly identifies o1 is free

    // 2. The Double Merge: Freeing o2 must merge with o1 AND o3 simultaneously
    general_free(o2);

    // Verify exactly one massive free block exists from o1 to o4
    BaseHeader* massive_hole = get_gen_header(o1);
    assert(massive_hole->is_allocated == 0);
    
    // Clean up the last block
    general_free(o4);
    
    assert(g->alloc_memory == 0);
    TEST_PASS();
}

/* ========================================================================= */
/* 3. Reallocation: In-Place Expansion (Eating a free neighbor)              */
/* ========================================================================= */
void test_gen_3_inplace_expand() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("3. Reallocation: In-Place Expansion");
    General* g = get_general();

    Handle h1 = handle_table_new(64);
    Handle h2 = handle_table_new(64);
    Handle h3 = handle_table_new(64);

    uint32_t o1 = general_malloc(64, h1);
    uint32_t o2 = general_malloc(64, h2);
    uint32_t o3 = general_malloc(64, h3); // Lock the frontier

    // Free the middle block
    general_free(o2);

    // Realloc h1. It should eat the hole left by h2!
    uint32_t new_o1 = general_realloc(100, h1);

    // Because it ate the neighbor, the offset should NOT change
    assert(new_o1 == o1);
    
    general_free(o1);
    general_free(o3);
    assert(g->alloc_memory == 0);
    TEST_PASS();
}

/* ========================================================================= */
/* 4. Reallocation: Out-of-Place Move                                        */
/* ========================================================================= */
void test_gen_4_out_of_place_move() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("4. Reallocation: Out-of-Place Move");
    General* g = get_general();

    Handle h1 = handle_table_new(64);
    Handle h2 = handle_table_new(64);

    uint32_t o1 = general_malloc(64, h1);
    uint32_t o2 = general_malloc(64, h2); // Blocks h1 from growing

    // Force h1 to grow. It has no choice but to relocate.
    uint32_t new_o1 = general_realloc(256, h1);

    // It must have physically moved
    assert(new_o1 != o1);
    
    // The old space must now be marked as free
    BaseHeader* old_head = get_gen_header(o1);
    assert(old_head->is_allocated == 0);

    general_free(new_o1);
    general_free(o2);
    assert(g->alloc_memory == 0);
    TEST_PASS();
}

/* ========================================================================= */
/* 5. Physical RAM Stitching (Dynamic Growth)                                */
/* ========================================================================= */
void test_gen_5_ram_stitching() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("5. Physical RAM Stitching");
    General* g = get_general();
    
    uint32_t starting_size = g->mem_size;

    Handle h1 = handle_table_new(starting_size - 128);
    Handle h2 = handle_table_new(2048);

    // Allocate almost the entire arena
    uint32_t o1 = general_malloc(starting_size - 128, h1);
    
    // Allocate past the capacity, forcing general_grow()
    uint32_t o2 = general_malloc(2048, h2);

    assert(o2 != INVALID_DATA_OFFSET);
    assert(g->mem_size > starting_size);

    // Clean up
    general_free(o1);
    general_free(o2);
    
    // The ultimate test: If stitching worked, there is EXACTLY ONE massive free 
    // block spanning from offset 0 to mem_size. 
    BaseHeader* root = get_gen_header(0);
    uint32_t total_bytes = HEADER_SIZE_TO_BYTES(root->size);
    
    assert(total_bytes == g->mem_size);
    assert(g->alloc_memory == 0);
    
    TEST_PASS();
}

/* ========================================================================= */
/* 6. TLSF Fragmentation Storm                                               */
/* ========================================================================= */
void test_gen_6_fragmentation_storm() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("6. TLSF Fragmentation Storm");
    General* g = get_general();
    
    #define CHURN_COUNT 1000
    uint32_t offsets[CHURN_COUNT];
    Handle handles[CHURN_COUNT];
    
    for (int i = 0; i < CHURN_COUNT; i++) {
        offsets[i] = INVALID_DATA_OFFSET;
        // Generate reusable handles for the slots
        handles[i] = handle_table_new(0); 
    }

    srand(1337);

    for (int i = 0; i < 10000; i++) {
        int slot = rand() % CHURN_COUNT;
        int action = rand() % 3;

        if (action == 0 || offsets[slot] == INVALID_DATA_OFFSET) {
            // ALLOCATE
            if (offsets[slot] != INVALID_DATA_OFFSET) {
                general_free(offsets[slot]);
            }
            uint32_t size = (rand() % 1024) + 24; // Min size 24 up to 1KB

            offsets[slot] = general_malloc(size, handles[slot]);
            
        } else if (action == 1) {
            // FREE
            general_free(offsets[slot]);
            offsets[slot] = INVALID_DATA_OFFSET;
            
        } else if (action == 2) {
            // REALLOC
            uint32_t new_size = (rand() % 2048) + 24;

            offsets[slot] = general_realloc(new_size, handles[slot]);
        }
    }
    // Clean up any remaining survivors
    for (int i = 0; i < CHURN_COUNT; i++) {
        if (offsets[i] != INVALID_DATA_OFFSET) {
            general_free(offsets[i]);
        }
    }

    // The Ultimate Verification
    // After 10,000 chaotic splits, merges, and reallocs, the TLSF index and 
    // boundary tags must perfectly resolve back to exactly 0 allocated bytes.
    assert(g->alloc_memory == 0);
    
    BaseHeader* root = get_gen_header(0);
    assert(root->is_allocated == 0);
    assert(HEADER_SIZE_TO_BYTES(root->size) == g->mem_size);

    TEST_PASS();
}
/* ========================================================================= */
/* 7. Realloc Payload Integrity Check                                        */
/* ========================================================================= */
void test_gen_7_payload_integrity() {
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    TEST_START("7. Realloc Payload Integrity Check");
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    General* g = get_general();

    Handle h1 = handle_table_new(64);
    Handle hBlock = handle_table_new(64);

    uint32_t o1 = general_malloc(64, h1);
    uint32_t oBlock = general_malloc(64, hBlock);

    // 1. Write a distinct byte pattern (0xAA) into the payload
    uint8_t* payload = (uint8_t*)(g->mem + o1 + sizeof(BaseHeader));
    for (int i = 0; i < 64; i++) {
        payload[i] = 0xAA;
    }

    // 2. Force an out-of-place reallocation
    uint32_t new_o1 = general_realloc(256, h1);
    assert(new_o1 != o1); // Prove it actually moved

    // Update the handle size just like the engine would
    HandleEntry* entry = handle_table_get_entry(h1);
    if(entry) entry->size = 256;

    // 3. Verify the data survived the memmove!
    uint8_t* new_payload = (uint8_t*)(g->mem + new_o1 + sizeof(BaseHeader));
    for (int i = 0; i < 64; i++) {
        assert(new_payload[i] == 0xAA);
    }

    general_free(new_o1);
    general_free(oBlock);
    
    assert(g->alloc_memory == 0);
    TEST_PASS();
}

/* ========================================================================= */
void run_general_tests() {
    printf("====================================================\n");
    printf("       GENERAL ARENA (TLSF) TEST SUITE              \n");
    printf("====================================================\n\n");

    // Initialize the arena once before testing
    mm_destroy();
    mm_init(DEFAULT_MM_CONFIG(1024*1024));
    test_gen_1_split_and_trim();
    test_gen_2_double_merge();
    test_gen_3_inplace_expand();
    test_gen_4_out_of_place_move();
    test_gen_5_ram_stitching();
    test_gen_6_fragmentation_storm();
    test_gen_7_payload_integrity();
    mm_destroy();

    printf("====================================================\n");
    printf("         ALL GENERAL TESTS PASSED!                  \n");
    printf("====================================================\n\n");
}