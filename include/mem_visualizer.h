#ifndef MEM_VISUALIZER_H
#define MEM_VISUALIZER_H

#include <stdint.h>

/* --- Arena Visualization --- */

/**
 * @brief High-level overview of all managed memory regions.
 * Displays the current state of both the Nursery and General Arena.
 */
void mm_visualize_allocator();

/**
 * @brief Detailed map of the Nursery (Bump Arena).
 * Visualizes the 'Frontier' (Bump Pointer) and distinguishes between 
 * Live, Dead, and Pinned objects.
 */
void mm_visualize_nursery();

/**
 * @brief Detailed map of the General Arena (TLSF).
 * Displays the heap layout, showing how free blocks are segregated into 
 * bins and how active blocks are distributed.
 */
void mm_visualize_general();

/* --- Indirection & Metadata Visualization --- */

/**
 * @brief Renders the current state of the Handle Table.
 * Shows which slots are active, their generation counters, and their 
 * current physical offsets.
 */
void mm_visualize_handle_table();

/* --- Graph & Connectivity Visualization --- */

/**
 * @brief Renders a logical node-link diagram of the object graph.
 * Displays how handles point to one another, independent of physical memory.
 */
void graph_visualize_all();

/**
 * @brief Renders the physical layout of the Edge Slab.
 * Shows how references are stored contiguously in the internal 
 * graph infrastructure.
 */
void graph_visualize_edges_physical();

/**
 * @brief Highlights cyclic dependencies and SCCs.
 * Groups objects by color based on their Strongly Connected Component, 
 * making it easy to see "Orphan Cycles" before they are collected.
 */
void graph_visualize_scc_clusters();

#endif /* MEM_VISUALIZER_H */