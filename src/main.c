#include "Interface/mem_manager.h"
#include "mem_visualizer.h"
#include "tests/nursery_stress.h"
#include "tests/general_stress.h"
#include "tests/graph_test.h"
#include <stdio.h>
#include <assert.h>

int main() {

    //run_nursery_tests();
    //run_general_tests();
    run_graph_tests();
    return 0;
}