#include "Arenas/tlsf.h"
#include <string.h>
#include <stdio.h>

// Internal Helper: Maps a size to its First Level (fl) and Second Level (sl) bin 
static void tlsf_mapping(uint32_t size, uint32_t* fl, uint32_t* sl) {
    *fl = 31 - __builtin_clz(size);
    *sl = (size >> (*fl - 4)) & 0xF;
}

int tlsf_init(TLSFAllocator* tlsf) {
    if (!tlsf) return 0;
    
    tlsf->fl_bitmap = 0;
    memset(tlsf->sl_bitmap, 0, sizeof(tlsf->sl_bitmap));
    
    for (uint32_t i = 0; i < TLSF_FL_INDEX_MAX; i++) {
        for (uint32_t j = 0; j < TLSF_SL_INDEX_COUNT; j++) {
            tlsf->blocks[i][j] = INVALID_DATA_OFFSET;
        }
    }
    return 1;
}

void tlsf_clear(TLSFAllocator* tlsf) {
    tlsf_init(tlsf);
}

void tlsf_insert(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset) {
    if (!tlsf || !mem_base || offset == INVALID_DATA_OFFSET) return;
    BaseHeader* header = (BaseHeader*)(mem_base + offset);
    uint32_t size = HEADER_SIZE_TO_BYTES(header->size);
    if(size< sizeof(FreeBlockHeader)+sizeof(BaseFooter))
        return;
    uint32_t fl, sl;
    tlsf_mapping(size, &fl, &sl);

    FreeBlockHeader* node = (FreeBlockHeader*)(mem_base + offset);
    
    node->prev_free = INVALID_DATA_OFFSET;
    node->next_free = tlsf->blocks[fl][sl];

    if (tlsf->blocks[fl][sl] != INVALID_DATA_OFFSET) {
        FreeBlockHeader* old_head = (FreeBlockHeader*)(mem_base + tlsf->blocks[fl][sl]);
        old_head->prev_free = offset;
    }
    tlsf->blocks[fl][sl] = offset;
    tlsf->fl_bitmap |= (1U << fl);
    tlsf->sl_bitmap[fl] |= (1U << sl);
}

void tlsf_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t offset) {
    if (!tlsf || !mem_base || offset == INVALID_DATA_OFFSET) return;

    FreeBlockHeader* node = (FreeBlockHeader*)(mem_base + offset);
    if(HEADER_SIZE_TO_BYTES(node->header.size)<sizeof(FreeBlockHeader)+sizeof(BaseFooter))
        return;
    if (node->prev_free != INVALID_DATA_OFFSET) {
        FreeBlockHeader* prev_node = (FreeBlockHeader*)(mem_base + node->prev_free);
        prev_node->next_free = node->next_free;
    } else {
        // We are the head of the list, update the array root
        uint32_t fl, sl;
        uint32_t size = HEADER_SIZE_TO_BYTES(node->header.size);
        tlsf_mapping(size, &fl, &sl);
        tlsf->blocks[fl][sl] = node->next_free;
        
        // If the list is now empty, turn off the bitmap flags
        if (tlsf->blocks[fl][sl] == INVALID_DATA_OFFSET) {
            tlsf->sl_bitmap[fl] &= ~(1U << sl);
            if (!tlsf->sl_bitmap[fl]) {
                tlsf->fl_bitmap &= ~(1U << fl);
            }
        }
    }

    if (node->next_free != INVALID_DATA_OFFSET) {
        FreeBlockHeader* next_node = (FreeBlockHeader*)(mem_base + node->next_free);
        next_node->prev_free = node->prev_free;
    }
}

uint32_t tlsf_find_and_remove(TLSFAllocator* tlsf, uint8_t* mem_base, uint32_t size) {
    if (!tlsf || !mem_base || size<(sizeof(FreeBlockHeader)+sizeof(BaseFooter))) return INVALID_DATA_OFFSET;

    uint32_t fl, sl;
    tlsf_mapping(size, &fl, &sl);
    size += (1U << (fl - 4)) - 1;
    tlsf_mapping(size, &fl, &sl);

    uint32_t sl_map = tlsf->sl_bitmap[fl] & (~0U << sl);
    uint32_t fl_map = (fl<31) ? (tlsf->fl_bitmap & (~0U << (fl + 1))) : 0;

    if (sl_map) {
        sl = __builtin_ctz(sl_map);
    } 
    else if (fl_map) {
        fl = __builtin_ctz(fl_map);
        sl = __builtin_ctz(tlsf->sl_bitmap[fl]);
    } 
    else {
        // No block large enough exists
        return INVALID_DATA_OFFSET;
    }

    uint32_t block_offset = tlsf->blocks[fl][sl];
    if (block_offset == INVALID_DATA_OFFSET) return INVALID_DATA_OFFSET; // Safety Catch

    tlsf_remove(tlsf, mem_base, block_offset);

    return block_offset;
}