#include "Interface/mem_manager.h"
#include "mem_visualizer.h"
#include <stdio.h>
#include <assert.h>

int main() {
    // Allocate a huge block
    Handle huge_block = mm_malloc(512 * 1024); // 512KB
    assert(huge_block.index != INVALID_INDEX);

    // Visualize the memory
    mm_visualize_allocator();

    // Free the huge block
    mm_free(huge_block);

    mm_destroy();

    printf("Huge arena visualizer test passed!\n");

    return 0;
}