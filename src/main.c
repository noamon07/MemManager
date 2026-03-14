#pragma warning(disable:4996)
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "heap.h"



int main() {
    printf("--- [HeapMaster] Starting Test Driver ---\n\n");

    // 1. Initialization
    printf("[Test 1] Initializing Heap...\n");
    mm_init();
    if (mm_checkheap(1)) {
        printf("-> Heap Initialized Successfully.\n\n");
    } else {
        printf("-> INIT FAILED.\n");
        return 1;
    }

    // 2. Basic Allocation
    printf("[Test 2] Allocating two blocks (p1, p2)...\n");
    char* p1 = (char*)mm_malloc(16);
    strcpy(p1, "Payload A"); // Write data to verify integrity later
    
    char* p2 = (char*)mm_malloc(16); // p2 blocks p1 from expanding
    strcpy(p2, "Payload B");

    printf("-> p1 at %p: %s\n", p1, p1);
    printf("-> p2 at %p: %s\n", p2, p2);
    mm_checkheap(0); // Silent check
    printf("-> Allocation successful.\n\n");

    // 3. Realloc Case C: Forced Move
    // p1 is 16 bytes. p2 is right after it.
    // If we ask for 64 bytes, p1 cannot expand. It MUST move.
    printf("[Test 3] Realloc p1 (Expansion Blocked -> Move)...\n");
    char* new_p1 = (char*)mm_realloc(p1, 64);

    if (new_p1 != p1) {
        printf("-> Success: Pointer moved from %p to %p.\n", p1, new_p1);
    } else {
        printf("-> FAILURE: Pointer stayed same (Overlap expected!).\n");
    }
    
    // Check Data Integrity
    if (strcmp(new_p1, "Payload A") == 0) {
        printf("-> Data Integrity OK: %s\n", new_p1);
    } else {
        printf("-> DATA CORRUPTION DETECTED!\n");
    }
    mm_checkheap(0);
    printf("\n");

    // 4. Realloc Case B: In-Place Expansion
    // Current state: [Old Free p1] [Alloc p2] [Alloc new_p1] ...
    // Let's free p2. Now new_p1 might have a free block *after* it depending on layout,
    // or we can allocate p3 and p4 to set up a specific merge scenario.
    
    // Let's reset for a clean expansion test.
    printf("[Test 4] Setting up In-Place Expansion...\n");
    mm_free(new_p1);
    mm_free(p2); 
    // Heap should be fully free now (except for internal fragmentation/alignment)
    
    char* p3 = (char*)mm_malloc(32);
    strcpy(p3, "ExpandMe");
    
    // Use malloc to consume the space immediately after p3 to block it? 
    // No, we want it to extend. So we leave the space after p3 free.
    
    printf("-> Requesting expansion of p3 (neighbor is free)...\n");
    char* new_p3 = (char*)mm_realloc(p3, 128);

    if (new_p3 == p3) {
        printf("-> Success: Pointer stayed in place at %p.\n", new_p3);
    } else {
        printf("-> Notice: Pointer moved. (This is valid, but missed optimization if neighbor was large enough).\n");
    }
    mm_checkheap(0);
    printf("\n");

    // 5. Final Cleanup
    printf("[Test 5] Freeing all and checking for total coalescing...\n");
    mm_free(new_p3);
    
    if (mm_checkheap(1)) {
        printf("-> Heap consistent. Test sequence complete.\n");
    }

    return 0;
}