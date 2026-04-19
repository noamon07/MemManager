#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "Interface/mem_manager.h"
#include "Infrastructure/handle.h"
#include "Infrastructure/graph.h"
#include "Infrastructure/scc_finder.h"
#include "Arenas/slab.h"
#include "mem_visualizer.h"

/* --- Test Helper Macros --- */
#define TEST_START(name) printf("[TEST] %s...\n", name)
#define TEST_PASS()      printf("  -> PASSED\n\n")


/* ========================================================================= */
/* Test Helpers                                                              */
/* ========================================================================= */

/* Safely create a dummy node for graph testing */
Handle create_graph_node() {
    // Allocate a small dummy chunk so the handle is mathematically valid
    Handle h = handle_table_new(16); 
    
    HandleEntry* entry = handle_table_get_entry(h);
    if (entry) {
        // Init default SCC state (normally your allocator handles this)
        entry->is_scc_root = 1;
        entry->next_in_scc = INVALID_HANDLE;
    }
    return h;
}

/* Verifies if the SCC engine successfully triggered mm_free on the object */
int is_dead(Handle h) {
    HandleEntry* entry = handle_table_get_entry(h);
    // Because mm_free calls handle_table_free(), the handle's generation increments.
    // Calling get_entry() with the old handle generation will return NULL!
    return (entry == NULL); 
}

/* ========================================================================= */
/* 1. The Basics: Standalone Object Deletion                                 */
/* ========================================================================= */
void test_graph_1_standalone() {
    mm_destroy();
    mm_init(4096);
    TEST_START("1. Standalone Object Deletion");
    
    Handle anchor = create_graph_node();
    write_handle(anchor);
    Handle target = create_graph_node();
    // Add reference: Anchor -> Target
    graph_add_ref(anchor, target);

    HandleEntry* t_entry = handle_table_get_entry(target);
    assert(t_entry->scc.external_in_degree == 1);
    assert(t_entry->in_degree == 1);
    // Sever the connection
    graph_remove_ref(anchor, target);
    // Target has 0 incoming connections. It must be instantly destroyed.
    assert(is_dead(target) == 1);
    
    // Anchor is still alive
    assert(is_dead(anchor) == 0);

    TEST_PASS();
}

/* ========================================================================= */
/* 2. The BFS Soft-Merge & Cluster Demolition (Death Spiral Check)           */
/* ========================================================================= */
void test_graph_2_cycle_merge() {
    mm_destroy();
    mm_init(4096);
    TEST_START("2. BFS Soft-Merge & Cluster Demolition");
    
    Handle anchor = create_graph_node();
    write_handle(anchor);
    Handle a = create_graph_node();
    Handle b = create_graph_node();
    Handle c = create_graph_node();
    

    // Line formation: Anchor -> A -> B -> C
    graph_add_ref(anchor, a);
    graph_add_ref(a, b);
    graph_add_ref(b, c);
    
    // B is still its own cluster root (not a cycle yet)
    assert(handle_table_get_entry(b)->is_scc_root == 1);

    // Close the loop: C -> A
    // This triggers the BFS Soft-Merge!
    graph_add_ref(c, a);
    // Verify perfect merge (All point to the same root)
    Handle root_a = get_scc_root(a);
    Handle root_b = get_scc_root(b);
    Handle root_c = get_scc_root(c);
    
    assert(handles_equal(root_a, root_b));
    assert(handles_equal(root_b, root_c));

    HandleEntry* master_root = handle_table_get_entry(root_a);
    
    // Internal cross-edges shouldn't count. The only anchor is Anchor -> A.
    assert(master_root->scc.external_in_degree == 1); 

    // THE DEATH SPIRAL CHECK: 
    // Deleting the anchor kills A, which kills the edge to B, which triggers
    // Tarjan unless the Demolition Check is working perfectly.
    graph_remove_ref(anchor, a);

    // The entire cycle should burn to the ground without crashing
    assert(is_dead(a) == 1);
    assert(is_dead(b) == 1);
    assert(is_dead(c) == 1);

    TEST_PASS();
}

/* ========================================================================= */
/* 3. The Rebuild: Tarjan Cycle Fracture                                     */
/* ========================================================================= */
void test_graph_3_tarjan_fracture() {
    mm_destroy();
    mm_init(4096);
    TEST_START("3. Tarjan Cycle Fracture & Partial Deletion");
    
    Handle anchor = create_graph_node();
    write_handle(anchor);
    Handle a = create_graph_node();
    Handle b = create_graph_node();
    Handle c = create_graph_node();

    // Build cycle: Anchor -> A -> B -> C -> A
    graph_add_ref(anchor, a);
    graph_add_ref(a, b);
    graph_add_ref(b, c);
    graph_add_ref(c, a);
    // FRACTURE THE CYCLE: Delete B -> C
    graph_remove_ref(b, c);
    // C is now completely orphaned from the graph. It must die.
    assert(is_dead(c) == 1);

    // A and B survive because (Anchor -> A -> B) still exists!
    assert(is_dead(a) == 0);
    assert(is_dead(b) == 0);

    // Tarjan should have repackaged them into standalone clusters
    assert(handle_table_get_entry(a)->is_scc_root == 1);
    assert(handle_table_get_entry(b)->is_scc_root == 1);

    TEST_PASS();
}

/* ========================================================================= */
/* 4. Self-Referential Loops (A -> A)                                        */
/* ========================================================================= */
void test_graph_4_self_reference() {
    mm_destroy();
    mm_init(4096);
    TEST_START("4. Self-Referential Cycles (A -> A)");
    
    Handle anchor = create_graph_node();
    write_handle(anchor);
    Handle a = create_graph_node();
    
    graph_add_ref(anchor, a);
    graph_add_ref(a, a); // Points to itself!
    
    Handle root_a = get_scc_root(a);
    HandleEntry* master_root = handle_table_get_entry(root_a);
    
    // The self-reference is an internal edge. External should still be 1.
    assert(master_root->scc.external_in_degree == 1);
    
    graph_remove_ref(anchor, a);
    
    // Deleting the external anchor must successfully break the self-loop
    assert(is_dead(a) == 1);
    
    TEST_PASS();
}

/* ========================================================================= */
/* 5. The Topology Nightmare (Orphaned Sub-Clusters)                         */
/* ========================================================================= */
void test_graph_5_complex_fracture() {
    mm_destroy();
    mm_init(4096);
    TEST_START("5. Complex Fracture (Orphaned Sub-Cluster)");
    
    Handle anchor = create_graph_node();
    write_handle(anchor);
    Handle a = create_graph_node();
    Handle b = create_graph_node();
    Handle c = create_graph_node();
    Handle d = create_graph_node();
    Handle e = create_graph_node();

    // Loop 1: Anchor -> A -> B -> C -> A
    graph_add_ref(anchor, a);
    graph_add_ref(a, b);
    graph_add_ref(b, c);
    graph_add_ref(c, a);

    // Loop 2: A -> D -> E -> D
    graph_add_ref(a, d);
    graph_add_ref(d, e);
    graph_add_ref(e, d);

    // Verify they correctly identified as two separate clusters
    assert(handles_equal(get_scc_root(a), get_scc_root(b)));
    assert(handles_equal(get_scc_root(d), get_scc_root(e)));
    assert(!handles_equal(get_scc_root(a), get_scc_root(d))); 

    HandleEntry* root_d = handle_table_get_entry(get_scc_root(d));
    assert(root_d->scc.external_in_degree == 1); // Only anchored by A -> D
    // Sever the arterial link between the two loops
    graph_remove_ref(a, d);

    // Loop 2 (D and E) lost its anchor to the game state. It must instantly die.
    assert(is_dead(d) == 1);
    assert(is_dead(e) == 1);

    // Loop 1 (A, B, C) survives!
    assert(is_dead(a) == 0);
    assert(is_dead(b) == 0);
    assert(is_dead(c) == 0);

    TEST_PASS();
}

void test_graph_6_high_churn_stress() {
    mm_destroy();
    mm_init(4096);
    TEST_START("6. High-Churn Graph Topology Stress Test");
    
    #define CHURN_COUNT 500
    Handle vm_registers[CHURN_COUNT];
    for (int i = 0; i < CHURN_COUNT; i++) {
        vm_registers[i] = (Handle){INVALID_INDEX, 0};
    }

    // Seed randomness for reproducibility
    srand(1337);

    for (int i = 0; i < 10000; i++) {
        int slot = rand() % CHURN_COUNT;
        int action = rand() % 4; // 4 possible chaotic actions
        if (action == 0 || !is_valid_handle(vm_registers[slot])) {
            // [ACTION 0: SPAWN & ROOT] 
            // The script creates a new object and stores it in a variable.
            if (is_valid_handle(vm_registers[slot])) {
                clear_handle(vm_registers[slot]); // Drop old root
            }
            vm_registers[slot] = create_graph_node(); 
            write_handle(vm_registers[slot]); // Pin the new root
        } 
        else if (action == 1) {
            // [ACTION 1: TANGLE]
            // Randomly connect two objects together.
            int target = rand() % CHURN_COUNT;
            if (is_valid_handle(vm_registers[target]) && slot != target) {
                // This will constantly trigger BFS Soft-Merges!
                graph_add_ref(vm_registers[slot], vm_registers[target]);
            }
        }
        else if (action == 2) {
            // [ACTION 2: FRACTURE]
            // Randomly sever a connection.
            int target = rand() % CHURN_COUNT;
            if (is_valid_handle(vm_registers[target])) {
                // graph_remove_ref safely ignores non-existent edges.
                // If it cuts an internal edge, it triggers a Tarjan Rebuild!
                graph_remove_ref(vm_registers[slot], vm_registers[target]);
            }
        }
        else if (action == 3) {
            // [ACTION 3: ABANDON]
            // The variable goes out of scope. 
            // The object might die instantly, OR it might survive because 
            // another object from Action 1 is still pointing to it!
            clear_handle(vm_registers[slot]);
            vm_registers[slot] = (Handle){INVALID_INDEX, 0}; 
        }
    }
    
    // [THE MASS EXTINCTION EVENT]
    // The game ends. Clear all remaining roots. 
    for (int i = 0; i < CHURN_COUNT; i++) {
        if (is_valid_handle(vm_registers[i])) {
            clear_handle(vm_registers[i]);
        }
    }
    
    // [THE ULTIMATE VALIDATION]
    // If your BFS, Tarjan's, Soft-Merges, and Demolition Checks work perfectly,
    // the massive, tangled 500-node graph will completely collapse upon itself.
    
    // 1. Check the Edge Slab
    extern Slab edges_slab;
    extern void* edges_memory_base;
    uint32_t active_edges = 0;
    for (uint32_t i = 0; i < edges_slab.slab_size; i += sizeof(Edge)) {
        Edge* e = (Edge*)((uint8_t*)edges_memory_base + i);
        if (is_valid_handle(e->child_handle)) {
            active_edges++;
        }
    }
    // 2. Check the Handle Table
    HandleTable* table = mm_get_handle_table_instance();
    uint32_t active_nodes = 0;
    for (uint32_t i = 0; i < table->size; i++) {
        HandleEntry* entry = handle_table_get_entry_by_index(i);
        if (entry && entry->is_allocated && entry->strategy != NULL) {
            active_nodes++;
        }
    }
    
    // If these asserts pass, your engine is leak-proof.
    assert(active_edges == 0 && "FATAL LEAK: Edges survived the extinction event!");
    assert(active_nodes == 0 && "FATAL LEAK: Nodes survived the extinction event!");

    TEST_PASS();
}
/* ========================================================================= */
void run_graph_tests() {
    printf("====================================================\n");
    printf("         GRAPH & SCC ARENA TEST SUITE               \n");
    printf("====================================================\n\n");

    // Initialize the systems (adjust sizes to your engine's specs)
    mm_init(4096);
    test_graph_1_standalone();
    test_graph_2_cycle_merge();
    test_graph_3_tarjan_fracture();
    test_graph_4_self_reference();
    test_graph_5_complex_fracture();
    test_graph_6_high_churn_stress();
    mm_destroy();
    printf("====================================================\n");
    printf("           ALL GRAPH TESTS PASSED!                  \n");
    printf("====================================================\n\n");
}