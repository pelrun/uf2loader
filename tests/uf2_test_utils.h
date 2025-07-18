#ifndef UF2_TEST_UTILS_H
#define UF2_TEST_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// UF2 constants and macros from Pico SDK's boot/uf2.h
#define UF2_MAGIC_START0 0x0A324655u
#define UF2_MAGIC_START1 0x9E5D5157u
#define UF2_MAGIC_END    0x0AB16F30u

#define UF2_FLAG_NOT_MAIN_FLASH          0x00000001u
#define UF2_FLAG_FILE_CONTAINER          0x00001000u
#define UF2_FLAG_FAMILY_ID_PRESENT       0x00002000u
#define UF2_FLAG_MD5_PRESENT             0x00004000u
#define UF2_FLAG_EXTENSION_FLAGS_PRESENT 0x00008000u

// Family IDs
#define RP2040_FAMILY_ID            0xe48bff56u
#define ABSOLUTE_FAMILY_ID          0xe48bff57u
#define DATA_FAMILY_ID              0xe48bff58u
#define RP2350_ARM_S_FAMILY_ID      0xe48bff59u
#define RP2350_RISCV_FAMILY_ID      0xe48bff5au
#define RP2350_ARM_NS_FAMILY_ID     0xe48bff5bu
#define FAMILY_ID_MAX               0xe48bff5bu

// UF2 block structure - must be exactly 512 bytes (sector size)
#pragma pack(push, 1)
typedef struct {
    // 32 byte header
    uint32_t magic_start0;
    uint32_t magic_start1;
    uint32_t flags;
    uint32_t target_addr;
    uint32_t payload_size;
    uint32_t block_no;
    uint32_t num_blocks;
    uint32_t file_size; // or familyID
    uint8_t data[476];  // 512 - 32 (header) - 4 (magic_end) = 476
    uint32_t magic_end;
} uf2_block_t;
#pragma pack(pop)

// Ensure the structure is exactly 512 bytes
static_assert(sizeof(uf2_block_t) == 512, "uf2_block_t must be exactly 512 bytes");

// Helper function to create a valid UF2 block
void create_uf2_block(uf2_block_t *block, uint32_t block_no, uint32_t num_blocks,
                     uint32_t target_addr, const uint8_t *data, uint32_t family_id);

// Helper function to initialize a UF2 block with default values
void init_uf2_block(uf2_block_t *block);

#endif // UF2_TEST_UTILS_H
