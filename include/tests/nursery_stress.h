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
void run_all_nursery_tests(void);
void test_bump_pointer_retreat(void);
void test_cascading_retreat(void);
void test_realloc_shrink(void);
void test_realloc_forward_merge_perfect_fit(void);
void test_realloc_backward_merge(void);
void test_defrag_single_gap(void);
void test_defrag_swiss_cheese(void);
void test_generational_promotion(void);
void test_heap_shift_arena_grow(void);
void test_double_free_and_dirty_calloc(void);
void test_chaotic_accounting_audit(void);
void test_out_of_memory(void);
#endif // NURSERY_STRESS_TESTS_H
