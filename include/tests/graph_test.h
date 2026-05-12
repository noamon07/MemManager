#ifndef GRAPH_TEST_H
#define GRAPH_TEST_H

/**
 * @brief Test 1: Standalone Reachability.
 * Verifies that a single object correctly tracks its in-degree and 
 * is reclaimed immediately when its last reference is cleared.
 */
void test_graph_1_standalone();

/**
 * @brief Test 2: Cycle Formation & Merge.
 * Connects multiple objects in a circular reference (A->B->C->A) 
 * and verifies that the SCC Finder correctly identifies them as 
 * a single atomic unit.
 */
void test_graph_2_cycle_merge();

/**
 * @brief Test 3: Tarjan's Fracture.
 * Breaks a link within an existing cycle and verifies that the 
 * algorithm correctly "fractures" the SCC into smaller components 
 * or standalone nodes.
 */
void test_graph_3_tarjan_fracture();

/**
 * @brief Test 4: Self-Reference.
 * Verifies that an object pointing to itself (A->A) does not 
 * create a memory leak and is correctly collected when external 
 * references are removed.
 */
void test_graph_4_self_reference();

/**
 * @brief Test 5: Complex Subgraph Fracture.
 * Simulates a complex web of interconnected SCCs. Verifies that 
 * removing a "Bridge" reference correctly triggers a cascade of 
 * collections for all now-unreachable sub-clusters.
 */
void test_graph_5_complex_fracture();

/**
 * @brief Main runner for the Reachability and Graph test suite.
 */
void run_graph_tests();

#endif /* GRAPH_TEST_H */