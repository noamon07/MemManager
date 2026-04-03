#ifndef STRATEGY_H
#define STRATEGY_H
#include "Infrastructure/handle.h"

typedef struct {
    uint32_t (*realloc)(uint32_t new_size, Handle handle);
    void  (*free)(uint32_t offset);
    void* (*get)(uint32_t offset);
} Strategy;

#endif