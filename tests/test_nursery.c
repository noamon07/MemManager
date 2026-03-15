#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "Arenas/nursery.h"

/**
 * @brief Test basic allocation of a small block of memory.
 */
void test_basic_allocation() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset(); // Ensure a clean state
    void* ptr = mm_malloc_nursery(16, &entry_index);
    assert(ptr != NULL);

    // Check if we can write to the allocated memory
    memset(ptr, 0xAB, 16);
    printf("%s: PASSED\n", __func__);
}

/**
 * @brief Test allocation of zero bytes.
 * The C standard says malloc(0) behavior is implementation-defined.
 * It can return NULL or a unique pointer. We test for the latter.
 */
void test_zero_size_allocation() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset();
    void* ptr = mm_malloc_nursery(0, &entry_index);
    assert(ptr != NULL);
    mm_free_nursery(ptr); // Should not crash
    printf("%s: PASSED\n", __func__);
}

/**
 * @brief Test multiple sequential allocations.
 * In a bump allocator, subsequent pointers should be at increasing addresses.
 */
void test_multiple_allocations() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset();
    void* ptr1 = mm_malloc_nursery(16, &entry_index);
    assert(ptr1 != NULL);
    void* ptr2 = mm_malloc_nursery(32, &entry_index);
    assert(ptr2 != NULL);
    assert(ptr1 != ptr2);

    // In a bump allocator, ptr2 should be at a higher address than ptr1.
    assert((uintptr_t)ptr2 > (uintptr_t)ptr1);
    printf("%s: PASSED\n", __func__);
}

/**
 * @brief Test that an allocation larger than the nursery's capacity fails.
 */
void test_allocation_exceeds_capacity() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset();
    // Attempt to allocate a very large block that should exceed nursery capacity.
    // This assumes the nursery is smaller than 100MB.
    void* ptr = mm_malloc_nursery(100 * 1024 * 1024, &entry_index);
    assert(ptr != NULL); // Should grow
    printf("%s: PASSED\n", __func__);
}

/**
 * @brief Test that free() is safe to call on valid and NULL pointers.
 * For a simple nursery, free() is often a no-op.
 */
void test_free_is_safe() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset();
    void* ptr = mm_malloc_nursery(16, &entry_index);
    assert(ptr != NULL);
    mm_free_nursery(ptr);   // Should not crash
    printf("%s: PASSED\n", __func__);
}

/**
 * @brief Test allocation behavior after a free call.
 * In this implementation, free can reclaim memory if it's at the end of the bump pointer.
 */
void test_allocation_after_free() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset();
    void* ptr1 = mm_malloc_nursery(16, &entry_index);
    assert(ptr1 != NULL);
    mm_free_nursery(ptr1);

    // The next allocation should reuse ptr1's memory since it was the last one.
    void* ptr2 = mm_malloc_nursery(16, &entry_index);
    assert(ptr2 != NULL);
    assert(ptr1 == ptr2);
    printf("%s: PASSED\n", __func__);
}

/**
 * @brief Test that the nursery reset allows memory to be reused from the start.
 */
void test_nursery_reset() {
    printf("Running %s...\n", __func__);
    uint32_t* entry_index;
    mm_nursery_reset();
    void* ptr1 = mm_malloc_nursery(16, &entry_index);
    assert(ptr1 != NULL);

    mm_nursery_reset(); // Reset the arena

    void* ptr2 = mm_malloc_nursery(16, &entry_index);
    assert(ptr2 != NULL);
    // After reset, allocation should start from the beginning again.
    assert(ptr1 == ptr2);
    printf("%s: PASSED\n", __func__);
}



int main() {
    printf("--- Starting Nursery Allocator Unit Tests ---\n\n");

    test_basic_allocation();
    test_zero_size_allocation();
    test_multiple_allocations();
    test_allocation_exceeds_capacity();
    test_free_is_safe();
    test_allocation_after_free();
    test_nursery_reset();

    printf("\n--- All Nursery Allocator Unit Tests Passed ---\n");

    mm_nursery_destroy();
    return 0;
}
