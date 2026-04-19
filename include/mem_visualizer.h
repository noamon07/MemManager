#ifndef MEM_VISUALIZER_H
#define MEM_VISUALIZER_H
#include <stdint.h>

void mm_visualize_allocator();
void mm_visualize_nursery();
void mm_visualize_general();
void graph_visualize_all();
void mm_visualize_handle_table();
void graph_visualize_edges_physical();
void graph_visualize_scc_clusters();
#endif
