#ifndef STACK_H
#define STACK_H

#include <stdint.h>

/**
 * @struct Stack
 * @brief Management structure for the generic stack.
 */
typedef struct {
    uint8_t* data;         /* The raw byte buffer of the stack */
    uint32_t element_size; /* Size of each individual element in bytes */
    uint32_t capacity;     /* Maximum capacity (number of elements) */
    uint32_t count;        /* Current number of elements stored */
} Stack;

/**
 * @brief Initializes a new stack instance.
 * 
 * Allocates the backing buffer based on capacity and element_size.
 * 
 * @param stack Pointer to the Stack structure to initialize.
 * @param capacity Maximum number of elements to hold.
 * @param element_size Size of one element (e.g., sizeof(uint32_t)).
 * @return 1 on success, 0 on memory allocation failure.
 */
int stack_init(Stack* stack, uint32_t capacity, uint32_t element_size);

/**
 * @brief Deallocates stack resources.
 * 
 * Frees the internal 'data' buffer and resets metadata.
 * 
 * @param s Pointer to the stack instance.
 */
void stack_destroy(Stack* s);

/**
 * @brief Pushes an element onto the top of the stack.
 * 
 * Copies element_size bytes from the provided pointer into the stack.
 * 
 * @param s Pointer to the stack instance.
 * @param element Pointer to the data to be copied onto the stack.
 * @return 1 on success, 0 if the stack is full.
 */
int stack_push(Stack* s, void* element);

/**
 * @brief Pops the top element from the stack.
 * 
 * Copies the top element into out_element and decrements the count.
 * 
 * @param s Pointer to the stack instance.
 * @param out_element Pointer to a buffer where the popped data will be stored.
 * @return 1 on success, 0 if the stack is empty.
 */
int stack_pop(Stack* s, void* out_element);

/**
 * @brief Retrieves the top element without removing it.
 * 
 * Provides a pointer to the internal stack memory for the top element.
 * 
 * @param s Pointer to the stack instance.
 * @param out_element Pointer to a void pointer that will receive the address of the top item.
 * @return 1 on success, 0 if the stack is empty.
 */
int stack_top(Stack* s, void** out_element);

#endif // STACK_H