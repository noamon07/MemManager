#ifndef STACK_H
#define STACK_H

#include <stdint.h>

typedef struct {
    uint8_t* data;         // The raw bytes of the stack
    uint32_t element_size; // How many bytes each item takes (e.g., sizeof(uint32_t))
    uint32_t capacity;     // Max number of elements
    uint32_t count;        // Current number of elements
} Stack;

// Creates a new stack
int stack_init(Stack* stack,uint32_t capacity, uint32_t element_size);

// Frees the memory
void stack_destroy(Stack* s);

// Core operations. We pass pointers to the data we want to push/pop.
// Returns true on success, false if stack is full/empty.
int stack_push(Stack* s, void* element);
int stack_pop(Stack* s, void* out_element);
int stack_top(Stack* s, void** out_element);
#endif // STACK_H