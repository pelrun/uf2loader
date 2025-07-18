#include "uf2_test_utils.h"
#include <string.h>

// Helper function to create a valid UF2 block
void create_uf2_block(uf2_block_t *block, uint32_t block_no, uint32_t num_blocks,
                     uint32_t target_addr, const uint8_t *data, uint32_t family_id) {
    // Initialize the block with zeros
    memset(block, 0, sizeof(uf2_block_t));
    
    // Set the UF2 magic numbers
    block->magic_start0 = UF2_MAGIC_START0;
    block->magic_start1 = UF2_MAGIC_START1;
    block->magic_end = UF2_MAGIC_END;
    
    // Set the block information
    block->flags = UF2_FLAG_FAMILY_ID_PRESENT;
    block->target_addr = target_addr;
    block->payload_size = 256; // Standard UF2 payload size
    block->block_no = block_no;
    block->num_blocks = num_blocks;
    block->file_size = family_id; // Reusing file_size field for family ID
    
    // Copy the data if provided
    if (data != NULL) {
        memcpy(block->data, data, block->payload_size);
    } else {
        // Fill with a pattern if no data provided
        for (int i = 0; i < block->payload_size; i++) {
            block->data[i] = (uint8_t)(i & 0xFF);
        }
    }
}
