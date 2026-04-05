#ifndef SCC_FINDER_H
#define SCC_FINDER_H

#include <stdint.h>
#include "Interface/mem_manager.h"

int scc_finder_init(uint32_t max_suspect_nodes);

void scc_finder_destroy();

void scc_process_suspect(Handle handle);

#endif