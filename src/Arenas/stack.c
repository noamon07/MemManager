#include "Arenas/stack.h"
#include <stdlib.h>
#include <string.h>

int stack_init(Stack* stack,uint32_t capacity, uint32_t element_size)
{
    if (!stack) return 0;

    // Allocate the raw byte array capable of holding everything
    stack->data = malloc(capacity * element_size);
    if (!stack->data) {
        return 0;
    }

    stack->element_size = element_size;
    stack->capacity = capacity;
    stack->count = 0;

    return 1;
}

void stack_destroy(Stack* s) {
    if (!s) return;
    free(s->data);
}

int stack_push(Stack* s, void* element) {
    if (s->count >= s->capacity) return 0;

    uint8_t* destination = s->data + (s->count * s->element_size);
    
    memcpy(destination, element, s->element_size);
    
    s->count++;
    return 1;
}

int stack_pop(Stack* s, void* out_element) {
    if (s->count == 0) 
    {
        return 0;
    }

    s->count--;
    
    uint8_t* source = s->data + (s->count * s->element_size);
    
    memcpy(out_element, source, s->element_size);
    
    return 1;
}

int stack_top(Stack* s, void** out_element)
{
    if (s->count == 0)
    {
        *out_element = NULL;
        return 0;
    }

    *out_element = s->data + ((s->count - 1) * s->element_size);
    return 1;
}