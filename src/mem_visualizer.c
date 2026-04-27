#include "mem_visualizer.h"
#include "Strategies/nursery.h"
#include "Infrastructure/handle.h"
#include "Strategies/general.h"
#include "Arenas/tlsf.h"
#include "Infrastructure/graph.h"
#include "Arenas/slab.h"
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
        int keep_printing = 1;
        for (size_t j = 0; j < 16 && keep_printing; j++) {
            if (i + j < len) {
                uint8_t c = byte_data[i + j];
                printf("%c", isprint(c) ? c : '.');
            } else {
                keep_printing = 0;
            }
        }
        printf("|\n");
    }
}


void mm_visualize_nursery() {
    printf("================================ [ NURSERY ARENA ] ================================\n");
    
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
    int keep_processing = 1;
    
    // Walk the contiguous blocks until we hit the current index
    while (current_offset < nursery->bump.cur_index && keep_processing) {
        
        BaseHeader* header = (BaseHeader*)(nursery->bump.mem + current_offset);
        
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(header->size);
        if (block_bytes == 0) {
            printf("[!] FATAL CORRUPTION: Block size is 0 at offset %08x!\n", current_offset);
            keep_processing = 0;
        } else {
            // Payload subtracts the Header
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
            
            // 3.Boundary Tag / Footer Reading
            BaseFooter* real_footer = (BaseFooter*)((uint8_t*)header + block_bytes - sizeof(BaseFooter));
            printf("    Boundary Tag (Footer Size): %08x\n\n", *real_footer);

            current_offset += block_bytes;
        }
    }
    
    // 4. Bump Pointer Indicator
    if (current_offset == nursery->bump.cur_index) {
        printf("[OFFSET: %08x]                                     <<< BUMP FRONTIER IS HERE\n", nursery->bump.cur_index);
    } else if (current_offset > nursery->bump.cur_index) {
        printf("[!] FATAL CORRUPTION: Block sizes exceeded cur_index!\n");
    }
    printf("===================================================================================\n\n");
}

// Walks the physical memory of the contiguous General Arena and maps TLSF holes.

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
    int keep_processing = 1;
    
    // Walk the contiguous blocks until we hit the absolute end of physical RAM
    while (current_offset < general->mem_size && keep_processing) {
        BaseHeader* header = (BaseHeader*)(general->mem + current_offset);
        uint32_t block_bytes = HEADER_SIZE_TO_BYTES(header->size);
        
        // Safety Catch: If block size is 0, we will infinite loop.
        if (block_bytes == 0) {
            printf("[!] FATAL CORRUPTION: Block size is 0 at offset %08x!\n", current_offset);
            keep_processing = 0;
        } else {
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
                       
                // Since it is free, we can peek inside the payload to see the TLSF linked list
                FreeBlockHeader* free_node = (FreeBlockHeader*)header;
                
                // Format INVALID_DATA_OFFSET nicely
                printf("    TLSF Links -> [PREV: ");
                if (free_node->prev_free == INVALID_DATA_OFFSET) printf("NONE    ] ");
                else printf("%08x] ", free_node->prev_free);
                
                printf("[NEXT: ");
                if (free_node->next_free == INVALID_DATA_OFFSET) printf("NONE    ]\n");
                else printf("%08x]\n", free_node->next_free);

                // Free blocks always have footers
                BaseFooter* real_footer = (BaseFooter*)((uint8_t*)header + block_bytes - sizeof(BaseFooter));
                printf("    Boundary Tag (Footer Size): %08x\n\n", *real_footer);
            }

            current_offset += block_bytes;
        }
    }
    // TLSF INDEX DUMP
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
extern Slab edges_slab;
extern void* edges_memory_base;

void mm_visualize_handle_table() 
{
    printf("================================ [ HANDLE TABLE (PHYSICAL) ] ============================\n");
    
    HandleTable* table = mm_get_handle_table_instance(); 
    if (!table || table->size == 0) {
        printf("Handle Table not initialized.\n");
        printf("=========================================================================================\n\n");
        return;
    }

    uint32_t max_handles = table->size;
    uint32_t live_nodes = 0;
    
    uint32_t consecutive_free_count = 0;
    uint32_t free_streak_start = 0;

    for (uint32_t i = 0; i < max_handles; i++) 
    {
        HandleEntry* entry = handle_table_get_entry_by_index(i);
        
        if (entry) {
            // Only dump full data for handles that are currently alive
            if (entry->is_allocated) 
            {
                // 1. If we were riding a streak of free blocks, print the truncated summary FIRST
                if (consecutive_free_count > 0) {
                    printf("[INDEX: %04u TO %04u] ... [ %u FREE FREELIST HANDLES TRUNCATED ] ...\n\n", 
                           free_streak_start, 
                           i - 1, 
                           consecutive_free_count);
                    consecutive_free_count = 0;
                }

                live_nodes++;
                
                printf("[INDEX: %04u] [GEN: %3u] [ *** ALLOCATED HANDLE *** ]\n", i, entry->generation);
                
                // --- MEMORY ROUTING INFO ---
                printf("    Memory: Strategy=%p | Data Offset=%08x | Size=%u bytes\n", 
                       (void*)entry->strategy, 
                       entry->data.data_offset, 
                       entry->size);
                
                // --- GRAPH TOPOLOGY INFO ---
                printf("    Graph : In-Degree=%-5u | First Edge Offset=%08x\n", 
                       entry->in_degree, 
                       entry->first_edge_offset);
                
                // --- SCC CLUSTER INFO (Handles the Union safely based on is_scc_root) ---
                if (entry->is_scc_root) {
                    printf("    SCC   : [ROOT] External In-Degree=%-5u | Next-In-SCC=[Idx:%u Gen:%u]\n", 
                           entry->scc.external_in_degree, 
                           entry->next_in_scc.index, 
                           entry->next_in_scc.generation);
                } else {
                    printf("    SCC   : [MEMBER] Root-SCC=[Idx:%u Gen:%u] | Next-In-SCC=[Idx:%u Gen:%u]\n", 
                           entry->scc.root_scc.index, 
                           entry->scc.root_scc.generation,
                           entry->next_in_scc.index, 
                           entry->next_in_scc.generation);
                }

                // --- BITFIELD FLAGS ---
                printf("    Flags : [Alloc:%d] [SCC_Root:%d] [SCC_Suspect:%d] [On_SCC_Stack:%d]\n",
                       entry->is_allocated, 
                       entry->is_scc_root, 
                       entry->is_scc_suspect, 
                       entry->is_on_scc_stack);
                
                // Optional: If you still want the raw hex dump of the 32-byte struct, uncomment this:
                // mem_dump(entry, sizeof(HandleEntry));
                
                printf("\n");
            } 
            else 
            {
                // The slot is free. Just quietly increment the streak counter.
                if (consecutive_free_count == 0) {
                    free_streak_start = i;
                }
                consecutive_free_count++;
            }
        }
    }
    
    // Catch any trailing free slots at the very end of the array
    if (consecutive_free_count > 0) {
        printf("[INDEX: %04u TO %04u] ... [ %u FREE FREELIST HANDLES TRUNCATED ] ...\n\n", 
               free_streak_start, 
               max_handles - 1, 
               consecutive_free_count);
    }

    printf("--- Handle Table Stats: %u Live Objects / %u Capacity ---\n", live_nodes, max_handles);
    printf("=========================================================================================\n\n");
}

void graph_visualize_edges_physical() 
{
    printf("================================ [ EDGES SLAB (PHYSICAL) ] ==============================\n");
    
    if (!edges_memory_base || edges_slab.slab_size == 0) {
        printf("Edges Slab not initialized.\n");
        printf("=========================================================================================\n\n");
        return;
    }

    printf("Edges Slab State: Start=%p, Total Size=%08x (Capacity: %lu edges)\n\n", 
           edges_memory_base, 
           edges_slab.slab_size,
           (unsigned long)(edges_slab.slab_size / sizeof(Edge)));

    uint32_t current_offset = 0;
    uint32_t consecutive_free_count = 0;
    uint32_t free_streak_start_offset = 0;
    
    while (current_offset < edges_slab.slab_size) 
    {
        Edge* edge = (Edge*)((uint8_t*)edges_memory_base + current_offset);
        
        // If child_handle is valid, the slot is currently housing live edge data
        if (is_valid_handle(edge->child_handle)) 
        {
            // 1. If we were riding a streak of free blocks, print the truncated summary FIRST
            if (consecutive_free_count > 0) {
                printf("[OFFSET: %08x TO %08x] ... [ %u FREE SLAB SLOTS TRUNCATED ] ...\n\n", 
                       free_streak_start_offset, 
                       current_offset - (uint32_t)sizeof(Edge), 
                       consecutive_free_count);
                consecutive_free_count = 0; // Reset the streak
            }

            // 2. Print the live allocated block
            printf("[OFFSET: %08x] [BLOCK_SIZE: %08zx] [ *** ALLOCATED EDGE *** ]\n", current_offset, sizeof(Edge));
            printf("    -> Points to Child: %u | Next Edge Offset: %08x\n", 
                   edge->child_handle.index, 
                   edge->next_edge_offset);
            mem_dump(edge, sizeof(Edge));
            printf("\n");
        } 
        else 
        {
            // The slot is free. Just quietly increment the streak counter.
            if (consecutive_free_count == 0) {
                free_streak_start_offset = current_offset;
            }
            consecutive_free_count++;
        }

        current_offset += sizeof(Edge);
    }
    
    // Catch any trailing free slots at the very end of the Slab memory block
    if (consecutive_free_count > 0) {
        printf("[OFFSET: %08x TO %08x] ... [ %u FREE SLAB SLOTS TRUNCATED ] ...\n\n", 
               free_streak_start_offset, 
               current_offset - (uint32_t)sizeof(Edge), 
               consecutive_free_count);
    }
    
    printf("=========================================================================================\n\n");
}
void graph_visualize_scc_clusters() 
{
    printf("================================ [ SCC CLUSTER TOPOLOGY ] ===============================\n");
    
    HandleTable* table = mm_get_handle_table_instance(); 
    if (!table || table->size == 0) {
        printf("Handle Table not initialized.\n");
        printf("=========================================================================================\n\n");
        return;
    }

    uint32_t total_clusters = 0;
    uint32_t max_handles = table->size;

    for (uint32_t i = 0; i < max_handles; i++) 
    {
        HandleEntry* root_entry = handle_table_get_entry_by_index(i);
        
        if (root_entry && root_entry->is_allocated && root_entry->is_scc_root) {
            total_clusters++;
            
            printf("====================================================\n");
            printf("|| CLUSTER ROOT: %04u | EXTERNAL IN_DEGREE: %-3u  ||\n", i, root_entry->scc.external_in_degree);
            printf("====================================================\n");
            
            if (root_entry->scc.external_in_degree == 0) {
                printf("|| [!] DEAD CLUSTER (Pending Destruction)         ||\n");
            }

            // Loop through the linked list to show all nodes trapped in this cluster
            printf("   Members: ");
            
            // Start traversal at the root
            Handle current_member = {i, root_entry->generation}; 
            uint32_t cluster_size = 0;
            int keep_traversing = 1;
            
            while (is_valid_handle(current_member) && keep_traversing) 
            {
                printf("[%04u] ", current_member.index);
                cluster_size++;
                
                HandleEntry* member_entry = handle_table_get_entry(current_member);
                if (!member_entry) {
                    keep_traversing = 0;
                } else {
                    // Draw arrows between members for readability
                    if (is_valid_handle(member_entry->next_in_scc)) {
                        printf("~~> ");
                    }
                    
                    current_member = member_entry->next_in_scc;
                }
            }
            
            printf("\n   Cluster Size: %u object(s)\n\n", cluster_size);
        }
    }
    
    printf("--- Engine Tracking %u Isolated Clusters ---\n", total_clusters);
    printf("=========================================================================================\n\n");
}
void graph_visualize_all() 
{
    printf("\n>>> INITIALIZING GRAPH MEMORY VISUALIZATION <<<\n\n");
    
    mm_visualize_handle_table();
    graph_visualize_edges_physical();
    graph_visualize_scc_clusters();
    
    printf(">>> END GRAPH VISUALIZATION <<<\n\n");
}

void mm_visualize_allocator() {
    printf("\n--- MEMORY MANAGER VISUALIZATION ---\n");
    
    mm_visualize_nursery();
    mm_visualize_general();

    printf("--- END VISUALIZATION ---\n\n");
}