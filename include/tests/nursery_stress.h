#ifndef NURSERY_STRESS_TESTS_H
#define NURSERY_STRESS_TESTS_H

/** --- Basic Bump Logic --- */

/** @brief Verifies the bump pointer advances and rolls back correctly on free. */
void test_1_frontier_rollback();

/** @brief Tests if adjacent free blocks are treated as a single gap during compaction. */
void test_2_bidirectional_coalesce();

/** --- Reallocation Scenarios --- */

/** @brief Verifies that shrinking an object in-place updates the frontier. */
void test_3_inplace_shrink();

/** @brief Tests in-place expansion when the frontier is clear. */
void test_4_frontier_expansion();

/** @brief Tests expansion by absorbing an adjacent dead block. */
void test_5_neighbor_absorption();

/** @brief Triggers a full relocation when in-place expansion is blocked. */
void test_6_full_relocation();

/** --- Garbage Collection & Promotion --- */

/** 
 * @brief THE CORE TEST: Sliding Compaction.
 * Verifies that live objects are slid to the start of the arena, 
 * eliminating all internal fragmentation.
 */
void test_7_sliding_compaction();

/** @brief Verifies that the 'generation' counter increments correctly per cycle. */
void test_8_generational_aging();

/** --- Robustness & Limits --- */

/** @brief Tests the arena's ability to resize its backing OS memory region. */
void test_9_dynamic_growth();

/** @brief Ensures all allocations and moves maintain strict 8-byte alignment. */
void test_10_byte_alignment();

/** @brief Verifies system behavior when the max_allowed_size is reached. */
void test_11_oom_fallback();

/** --- Integration & Integrity --- */

/** @brief Simulates thousands of rapid allocations/frees to detect race conditions. */
void test_12_high_churn_stress();

/** @brief Crucial check: ensures data bits remain identical after a physical move. */
void test_13_realloc_payload_integrity();

/** @brief Main runner for the Nursery validation suite. */
void run_nursery_tests();

#endif /* NURSERY_STRESS_TESTS_H */