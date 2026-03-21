#include "mem_visualizer.h"
#include "Strategies/nursery.h"
#include "Arenas/handle.h"
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

/*
 * mem_dump
 * Dumps a region of memory in a classic hex+ASCII format.
 */
void mem_dump(const void* data, size_t len) {
    if (!data || len == 0) return;
    
    const uint8_t* byte_data = (const uint8_t*)data;
    for (size_t i = 0; i < len; i += 16) {
        // Print offset
        printf("    %08zx: ", (uintptr_t)byte_data + i);

        // Print hex values
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                printf("%02x ", byte_data[i + j]);
            } else {
                printf("   ");
            }
            if (j == 7) {
                printf(" ");
            }
        }
        printf(" |");

        // Print ASCII values
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) {
                uint8_t c = byte_data[i + j];
                printf("%c", isprint(c) ? c : '.');
            } else {
                break;
            }
        }
        printf("|\n");
    }
}

/*
 * mm_visualize_nursery
 * Prints a map of the nursery arena.
 */
void mm_visualize_nursery() {
    printf("================================ [ NURSERY ARENA ] ================================\n");
    
    // Use the updated getter from your nursery implementation
    Nursery* nursery = get_nursery_instance(); 
    
    // Check the underlying bump allocator's memory pointer
    if (!nursery || !nursery->bump.mem) {
        printf("Nursery not initialized.\n");
        printf("===================================================================================\n\n");
        return;
    }

    // Print global arena state from the bump struct
    printf("Nursery State: Start=%p, Size=%u, Bump Pointer (Offset)=%u, Allocated Mem=%u\n\n", 
           (void*)nursery->bump.mem, 
           nursery->bump.mem_size, 
           nursery->bump.cur_index,
           nursery->bump.alloc_memory);

    uint32_t current_offset = 0;
    
    // Walk the contiguous blocks until we hit the frontier
    while (current_offset < nursery->bump.cur_index) {
        // Cast directly to your actual BaseHeader struct
        BaseHeader* header = (BaseHeader*)(nursery->bump.mem + current_offset);
        
        // Use your native macro to get exact byte size
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(header->size);
        
        // Payload subtracts the Header AND the Footer
        uint32_t payload_size = block_bytes - sizeof(BaseHeader);

        // 1. Detailed Metadata Print
        // custom_flags acts as your generation counter
        printf("[OFFSET: %08x] [A:%d B:%d] [BLOCK_SIZE: %-5u] [PAYLOAD: %-5u] [GEN: %d] [HANDLE: %u, GENERATION: %u]\n", 
               current_offset, 
               header->is_allocated,
               header->before_alloc,
               block_bytes, 
               payload_size, 
               header->custom_flags,
               header->handle.index,
               header->handle.generation); 

        // 2. Hex Dump (Only dump if allocated to avoid printing old garbage data)
        if (header->is_allocated) {
            void* payload = (void*)(header + 1);
            size_t dump_size = payload_size > 64 ? 64 : payload_size;
            mem_dump(payload, dump_size);

            if (payload_size > 64) {
                printf("    ... [TRUNCATED] ...\n");
            }
        } else {
            printf("    [ FREE MEMORY HOLE ]\n");
        }
        
        // 3. True Boundary Tag / Footer Reading
        // Because you implemented PUT_FOOTER for both allocated and free blocks,
        // we can safely read the footer for EVERY block now.
        BaseFooter* real_footer = (BaseFooter*)((uint8_t*)header + block_bytes - sizeof(BaseFooter));
        printf("    Boundary Tag (Footer Size): %u\n\n", *real_footer);

        current_offset += block_bytes;
    }
    
    // 4. Bump Pointer Indicator
    if (current_offset == nursery->bump.cur_index) {
        printf("[OFFSET: %08x]                                     <<< BUMP FRONTIER IS HERE\n", nursery->bump.cur_index);
    } else if (current_offset > nursery->bump.cur_index) {
        printf("[!] FATAL CORRUPTION: Block sizes exceeded cur_index!\n");
    }
    printf("===================================================================================\n\n");
}

/*
 * mm_visualize_allocator
 * The main visualization function.
 */
void mm_visualize_allocator() {
    printf("\n--- MEMORY MANAGER VISUALIZATION ---\n");
    
    mm_visualize_nursery();

    // mm_visualize_long_lived(); // Ready for the TLSF visualizer

    printf("--- END VISUALIZATION ---\n\n");
}