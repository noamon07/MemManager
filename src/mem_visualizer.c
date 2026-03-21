#include "mem_visualizer.h"
#include "Strategies/nursery.h"
#include "Arenas/handle.h"
#include "Strategies/general.h"
#include "Arenas/tlsf.h"
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
    printf("Nursery State: Start=%p, Size=%08x, Bump Pointer (Offset)=%08x, Allocated Mem=%08x\n\n", 
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
        if (block_bytes == 0) {
            printf("[!] FATAL CORRUPTION: Block size is 0 at offset %08x!\n", current_offset);
            break;
        }
        // Payload subtracts the Header AND the Footer
        uint32_t payload_size = block_bytes - sizeof(BaseHeader);

        // 1. Detailed Metadata Print
        // custom_flags acts as your generation counter
        printf("[OFFSET: %08x] [A:%d B:%d] [BLOCK_SIZE: %08x] [PAYLOAD: %08x] [GEN: %d] [HANDLE: %u, GENERATION: %u]\n", 
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
        printf("    Boundary Tag (Footer Size): %08x\n\n", *real_footer);

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
 * mm_visualize_general
 * Walks the physical memory of the contiguous General Arena and maps TLSF holes.
 */
void mm_visualize_general() {
    printf("================================ [ GENERAL ARENA ] ================================\n");
    
    General* general = get_general_instance(); 
    
    if (!general || !general->mem) {
        printf("General Arena not initialized.\n");
        printf("===================================================================================\n\n");
        return;
    }

    printf("General State: Start=%p, Total Size=%08x, Allocated Mem=%08x\n\n", 
           (void*)general->mem, 
           general->mem_size, 
           general->alloc_memory);

    uint32_t current_offset = 0;
    
    // Walk the contiguous blocks until we hit the absolute end of physical RAM
    while (current_offset < general->mem_size) {
        BaseHeader* header = (BaseHeader*)(general->mem + current_offset);
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(header->size);
        
        // Safety Catch: If block size is 0, we will infinite loop.
        if (block_bytes == 0) {
            printf("[!] FATAL CORRUPTION: Block size is 0 at offset %08x!\n", current_offset);
            break;
        }

        uint32_t payload_size = block_bytes - sizeof(BaseHeader);

        if (header->is_allocated) {
            // Allocated Block Metadata
            printf("[OFFSET: %08x] [A:%d B:%d] [BLOCK_SIZE: %08x] [PAYLOAD: %08x] [HANDLE: %u, GEN: %u]\n", 
                   current_offset, 
                   header->is_allocated,
                   header->before_alloc,
                   block_bytes, 
                   payload_size, 
                   header->handle.index,
                   header->handle.generation); 
                   
            void* payload = (void*)(header + 1);
            size_t dump_size = payload_size > 64 ? 64 : payload_size;
            mem_dump(payload, dump_size);

            if (payload_size > 64) {
                printf("    ... [TRUNCATED] ...\n");
            }
            printf("\n");
        } 
        else {
            // Free Block Metadata
            printf("[OFFSET: %08x] [A:%d B:%d] [BLOCK_SIZE: %08x] [PAYLOAD: %08x] [ *** FREE HOLE *** ]\n", 
                   current_offset, 
                   header->is_allocated,
                   header->before_alloc,
                   block_bytes, 
                   payload_size); 
                   
            // Since it is free, we can peek inside the payload to see the TLSF linked list!
            FreeBlockHeader* free_node = (FreeBlockHeader*)header;
            
            // Format INVALID_DATA_OFFSET nicely
            printf("    TLSF Links -> [PREV: ");
            if (free_node->prev_free == INVALID_DATA_OFFSET) printf("NONE    ] ");
            else printf("%08x] ", free_node->prev_free);
            
            printf("[NEXT: ");
            if (free_node->next_free == INVALID_DATA_OFFSET) printf("NONE    ]\n");
            else printf("%08x]\n", free_node->next_free);

            // Free blocks ALWAYS have footers in our Freelist architecture
            BaseFooter* real_footer = (BaseFooter*)((uint8_t*)header + block_bytes - sizeof(BaseFooter));
            printf("    Boundary Tag (Footer Size): %08x\n\n", *real_footer);
        }

        current_offset += block_bytes;
    }
    /* --- TLSF INDEX DUMP --- */
    printf("\n[ TLSF FREE LIST HEADS ]\n");
    
    // Check if the entire allocator is full (no free blocks)
    if (general->tlsf.fl_bitmap == 0) {
        printf("  All TLSF bins are currently empty (NULL).\n");
    } else {
        // Iterate through all First Level (FL) bins
        for (uint32_t fl = 0; fl < TLSF_FL_INDEX_MAX; fl++) {
            
            // Only print this FL row if its bitmap flag is flipped on
            if ((general->tlsf.fl_bitmap & (1U << fl)) != 0) {
                printf("  FL %2u: ", fl);
                
                // Iterate through all 16 Second Level (SL) bins for this FL
                for (uint32_t sl = 0; sl < TLSF_SL_INDEX_COUNT; sl++) {
                    uint32_t block_offset = general->tlsf.blocks[fl][sl];
                    
                    if (block_offset == INVALID_DATA_OFFSET) {
                        printf("[NULL    ] ");
                    } else {
                        printf("[%08x] ", block_offset);
                    }
                }
                printf("\n");
            }
        }
    }
    if (current_offset > general->mem_size) {
        printf("[!] FATAL CORRUPTION: Block sizes exceeded mem_size! (Overflowed by %08x bytes)\n", 
               current_offset - general->mem_size);
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
    mm_visualize_general();
    // mm_visualize_long_lived(); // Ready for the TLSF visualizer

    printf("--- END VISUALIZATION ---\n\n");
}