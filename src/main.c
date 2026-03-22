#include "Interface/mem_manager.h"
#include "mem_visualizer.h"
#include "tests/nursery_stress.h"
#include "tests/general_stress.h"
#include <stdio.h>
#include <assert.h>

int main() {

    run_nursery_tests();
    run_general_tests();
    // test_gc_cyclic_leak();

    return 0;
}