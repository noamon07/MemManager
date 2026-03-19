#include "mem_visualizer.h"
#include "Arenas/nursery.h"
#include "Arenas/handle.h"
#include "Arenas/general.h"
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

// Block header structure from nursery.c
typedef struct {
    uint32_t size:28,
             is_allocated:1,
             before_alloc:1,
             generation:2;
    uint32_t entry_index;
} BlockHeader;

#define HEADER_SIZE_TO_SIZE(header_size) ((uint32_t)((header_size)<<3))

/*
 * mem_dump
 * Dumps a region of memory in a classic hex+ASCII format.
 */
void mem_dump(const void* data, size_t len) {
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
    Nursery* nursery = mm_get_nursery_instance();
    if (nursery->mem == NULL) {
        printf("Nursery not initialized.\n");
        return;
    }

    printf("Nursery State: Start=%p, Size=%u, Bump Pointer (Offset)=%u\n\n", 
           (void*)nursery->mem, nursery->mem_size, nursery->cur_index);

    uint32_t current_offset = 0;
    while (current_offset < nursery->cur_index) {
        BlockHeader* header = (BlockHeader*)(nursery->mem + current_offset);
        uint32_t block_size = HEADER_SIZE_TO_SIZE(header->size);
        uint32_t payload_size = block_size - sizeof(BlockHeader);

        // 1. Detailed Metadata Print (Bits, Raw Size, Calculated Size, Payload)
        printf("[OFFSET: %08x] [A:%d B:%d] [BLOCK_SIZE: %-5u (RAW: %u)] [PAYLOAD: %-5u] [GEN: %d]\n", 
               current_offset, 
               header->is_allocated,
               header->before_alloc,
               block_size, 
               header->size, // The literal 28-bit value stored in the struct
               payload_size, 
               header->generation);

        // 2. Bump Pointer Indicator
        if (current_offset + block_size == nursery->cur_index) {
             printf("                                                     <<< BUMP POINTER >>>\n");
        }

        // 3. Hex Dump
        void* payload = (void*)(header + 1);
        size_t dump_size = payload_size > 64 ? 64 : payload_size;
        mem_dump(payload, dump_size);

        if (payload_size > 64) {
            printf("    ... [TRUNCATED] ...\n");
        }
        
        // 4. True Boundary Tag / Footer Reading
        if (!header->is_allocated) {
            // Step to the very end of the block and read the last 4 bytes
            uint32_t* real_footer = (uint32_t*)((char*)header + block_size - sizeof(uint32_t));
            printf("    Boundary Tag (Real): %u\n", *real_footer);
        } else {
            printf("    Boundary Tag: [None - Block is Allocated]\n");
        }

        current_offset += block_size;
    }
     if (current_offset >= nursery->cur_index) {
             printf("\n[OFFSET: %08x]                                     <<< BUMP POINTER IS HERE\n", nursery->cur_index);
    }
    printf("===================================================================================\n\n");
}

void mm_visualize_huge() {
    printf("================================== [ HUGE ARENA ] =================================\n");
    HandleTable* table = mm_get_handle_table_instance();
    if (table == NULL) {
        printf("Handle table not initialized.\n");
        return;
    }

    printf("Scanning Handle Table for HUGE allocations...\n\n");

    for (uint32_t i = 0; i < table->size; ++i) {
        HandleEntry* entry = &table->entries[i];
        if (entry->is_allocated && entry->stratigy_id == ALLOC_TYPE_HUGE) {
            void* ptr = entry->data.data_ptr.ptr;
            printf("[HANDLE INDEX: %08x] [PTR: %p] [SIZE: %-10u]\n", 
                   i, ptr, entry->size);
            
            size_t dump_size = entry->size > 64 ? 64 : entry->size;
            mem_dump(ptr, dump_size);
            if (entry->size > 64) {
                printf("    ... [TRUNCATED] ...\n");
            }
            printf("\n");
        }
    }
    printf("===================================================================================\n\n");
}

/*
 * mm_visualize_general
 * Prints a map of the General (TLSF) arena, including linked list states.
 */
void mm_visualize_general() {
    printf("================================ [ GENERAL ARENA ] ================================\n");
    General* general_arena = mm_get_general_instance();
    if (general_arena->mem == NULL) {
        printf("General Arena not initialized.\n");
        return;
    }

    printf("General State: Start=%p, Total Size=%u, Allocated Memory=%u\n\n", 
           (void*)general_arena->mem, general_arena->mem_size, general_arena->alloc_memory);

    uint32_t current_offset = 0;
    
    // Unlike the nursery, we scan the entire memory pool up to mem_size
    while (current_offset < general_arena->mem_size) {
        BlockHeader* header = (BlockHeader*)(general_arena->mem + current_offset);
        uint32_t block_size = HEADER_SIZE_TO_SIZE(header->size);
        uint32_t payload_size = block_size - sizeof(BlockHeader);

        // Failsafe to prevent infinite loops if memory gets corrupted
        if (block_size == 0) {
            printf("[ERROR] Block size is 0 at offset %08x. Aborting dump.\n", current_offset);
            break;
        }

        // 1. Detailed Metadata Print
        printf("[OFFSET: %08x] [A:%d B:%d] [BLOCK_SIZE: %-5u (RAW: %u)] [GEN: %d]\n", 
               current_offset, 
               header->is_allocated,
               header->before_alloc,
               block_size, 
               header->size, 
               header->generation);

        if (header->is_allocated) {
            // 2. Hex Dump for Allocated Blocks
            printf("    [PAYLOAD: %-5u]\n", payload_size);
            void* payload = (void*)(header + 1);
            size_t dump_size = payload_size > 64 ? 64 : payload_size;
            mem_dump(payload, dump_size);

            if (payload_size > 64) {
                printf("    ... [TRUNCATED] ...\n");
            }
        } else {
            // 3. TLSF Details for Free Blocks
            printf("    [FREE BLOCK] TLSF Links -> Prev: %08x | Next: %08x\n", 
                   *(uint32_t*)((char*)header+sizeof(BlockHeader)), *(uint32_t*)((char*)header+sizeof(BlockHeader)+sizeof(uint32_t)));        
            
            // Footer reading
            uint32_t* real_footer = (uint32_t*)((char*)header + block_size - sizeof(uint32_t));
            printf("    Boundary Tag (Real Footer): %u\n", *real_footer);
        }

        current_offset += block_size;
        printf("-----------------------------------------------------------------------------------\n");
    }
    printf("===================================================================================\n\n");
}

/*
 * mm_visualize_allocator
 * The main visualization function.
 */
void mm_visualize_allocator() {
    printf("\n--- MEMORY MANAGER VISUALIZATION ---\n");
    
    // For now, we only have the nursery visualizer
    mm_visualize_nursery();
    mm_visualize_huge();

    // Later, we would add visualizations for other arenas:
    // mm_visualize_old_gen();
    // mm_visualize_huge_allocs();

    printf("--- END VISUALIZATION ---\n\n");
}
