#include "Interface/mem_manager.h"
#include "mem_visualizer.h"
#include "tests/nursery_stress.h"
#include <stdio.h>
#include <assert.h>

int main() {
    // Run the comprehensive nursery stress tests first
    run_all_tests();
    return 0;
}