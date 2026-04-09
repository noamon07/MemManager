#ifndef SCC_FINDER_H
#define SCC_FINDER_H

#include <stdint.h>
#include "Interface/mem_manager.h"

int scc_finder_init(uint32_t max_suspect_nodes);

void scc_finder_destroy();

void recalculate_scc_subgraph(Handle start_handle);

#endif