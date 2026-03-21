#ifndef NURSERY_STRESS_TESTS_H
#define NURSERY_STRESS_TESTS_H

/**
 * @brief Runs a comprehensive suite of stress tests on the nursery allocator.
 * 
 * This function executes a series of 12 specific tests designed to validate
 * the functionality, correctness, and robustness of the nursery allocator
 * under various conditions, including allocation, deallocation, reallocation,
 * defragmentation, and boundary cases.
 */
void run_nursery_tests();
void test_1_frontier_rollback();
void test_2_bidirectional_coalesce();
void test_3_inplace_shrink();
void test_4_frontier_expansion();
void test_5_neighbor_absorption();
void test_6_full_relocation();
void test_7_sliding_compaction();
void test_8_generational_aging();
void test_9_dynamic_growth();
void test_10_byte_alignment();
void test_11_oom_fallback();
void test_12_high_churn_stress();
void test_13_realloc_payload_integrity();
#endif // NURSERY_STRESS_TESTS_H
