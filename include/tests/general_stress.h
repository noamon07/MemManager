#ifndef GENERAL_STRESS_H
#define GENERAL_STRESS_H

/**
 * @brief Test 1: Block Splitting & Trimming.
 * Allocates a large block and resizes it downward, verifying that the 
 * "trimmed" remainder is correctly returned to the TLSF free lists.
 */
void test_gen_1_split_and_trim();

/**
 * @brief Test 2: Double Coalescing (Merge).
 * Frees a block situated between two other free blocks, verifying that 
 * the allocator merges all three into a single contiguous free block.
 */
void test_gen_2_double_merge();

/**
 * @brief Test 3: In-Place Expansion.
 * Attempts to realloc an object upward when the subsequent block is free, 
 * verifying that the allocator expands the block without moving the data.
 */
void test_gen_3_inplace_expand();

/**
 * @brief Test 4: Out-of-Place Movement.
 * Triggers a realloc when the neighbor is occupied, verifying that the 
 * system moves the object, updates the Handle Table, and frees the old slot.
 */
void test_gen_4_out_of_place_move();

/**
 * @brief Test 5: RAM Stitching.
 * Tests the OS abstraction layer's ability to request new regions and 
 * "stitch" them into the existing TLSF structure when the arena is full.
 */
void test_gen_5_ram_stitching();

/**
 * @brief Test 6: Fragmentation Storm.
 * Performs thousands of random-sized allocations and deallocations to 
 * stress the TLSF bitmask searching and binning logic.
 */
void test_gen_6_fragmentation_storm();

/**
 * @brief Test 7: Payload Integrity.
 * Fills allocated blocks with known patterns (magic numbers) and 
 * verifies they remain unchanged after various GC and realloc operations.
 */
void test_gen_7_payload_integrity();

/**
 * @brief Main runner for the General Arena test suite.
 */
void run_general_tests();

#endif /* GENERAL_STRESS_H */