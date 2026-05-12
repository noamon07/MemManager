#ifndef STRATEGY_H
#define STRATEGY_H

#include "Infrastructure/handle.h"

/**
 * @struct Strategy
 * @brief A virtual function table (VTable) for memory arena operations.
 */
typedef struct {
    /**
     * @brief Arena-specific reallocation logic.
     * @param new_size The requested new size for the block.
     * @param handle The handle being resized.
     * @return uint32_t New physical offset within the respective arena.
     */
    uint32_t (*realloc)(uint32_t new_size, Handle handle);

    /**
     * @brief Arena-specific deallocation logic.
     * @param offset The physical offset to be released.
     */
    void  (*free)(uint32_t offset);

    /**
     * @brief Arena-specific pointer resolution logic.
     * Maps an offset to a physical memory address based on the arena's base.
     * @param offset The physical offset to resolve.
     * @return void* The resolved physical pointer.
     */
    void* (*get)(uint32_t offset);
} Strategy;

#endif /* STRATEGY_H */