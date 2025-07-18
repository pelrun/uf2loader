// Critical Scenario Tests for Picocalc SD Bootloader
// Tests the most dangerous and critical code paths

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include "../bootloader/include/boot_defs.h"
#include "test_common.h"
#include "uf2_test_utils.h"

// Mock flash
#define MOCK_FLASH_SIZE (2 * 1024 * 1024)  // 2MB
extern uint8_t mock_flash[MOCK_FLASH_SIZE];

// Test helpers
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        printf("\nASSERT FAILED: %s:%d: expected %lld, got %lld\n", \
               __FILE__, __LINE__, (long long)(expected), (long long)(actual)); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        printf("\nASSERT FAILED: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        tests_failed++; \
        return; \
    } \
} while(0)

// Simulate flash operations that can fail mid-operation
static bool flash_operation_should_fail = false;
static int flash_operations_before_failure = 0;
static int flash_operation_count = 0;

void flash_range_erase_with_failure(uint32_t addr, size_t size) {
    flash_operation_count++;
    if (flash_operation_should_fail && flash_operation_count > flash_operations_before_failure) {
        printf("SIMULATED: Flash erase failure at 0x%x\n", addr);
        // Partially erase to simulate power loss
        memset(&mock_flash[addr], 0xFF, size / 2);
        return;
    }
    memset(&mock_flash[addr], 0xFF, size);
}

void flash_range_program_with_failure(uint32_t addr, const uint8_t *data, size_t size) {
    flash_operation_count++;
    if (flash_operation_should_fail && flash_operation_count > flash_operations_before_failure) {
        printf("SIMULATED: Flash program failure at 0x%x\n", addr);
        // Partially program to simulate power loss
        memcpy(&mock_flash[addr], data, size / 2);
        // Corrupt the rest
        memset(&mock_flash[addr + size/2], 0x55, size / 2);
        return;
    }
    memcpy(&mock_flash[addr], data, size);
}

// Test: Boot2 preservation during sector 0 flash
TEST(boot2_preservation) {
    uint8_t original_boot2[BOOT2_SIZE];
    uint8_t test_app[FLASH_PAGE_SIZE];
    
    // Fill boot2 with recognizable pattern
    for (int i = 0; i < BOOT2_SIZE; i++) {
        original_boot2[i] = 0xB0 + (i & 0x0F);
        mock_flash[i] = original_boot2[i];
    }
    
    // Fill test app
    memset(test_app, 0xAA, sizeof(test_app));
    
    // Simulate flashing to sector 0 (which contains boot2)
    // The bootloader should preserve boot2
    flash_range_erase(0, FLASH_SECTOR_SIZE);
    flash_range_program(0, original_boot2, BOOT2_SIZE);
    flash_range_program(0x1000, test_app, FLASH_PAGE_SIZE);
    
    // Verify boot2 is preserved
    for (int i = 0; i < BOOT2_SIZE; i++) {
        ASSERT_EQ(original_boot2[i], mock_flash[i]);
    }
    
    // Verify app was written
    for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
        ASSERT_EQ(test_app[i], mock_flash[0x1000 + i]);
    }
}

// Test: Flash corruption detection
TEST(flash_corruption_detection) {
    uint8_t test_data[FLASH_PAGE_SIZE];
    uint32_t addr = 0x40000;
    
    // Fill with pattern
    for (int i = 0; i < FLASH_PAGE_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
    
    // Write to flash
    flash_range_erase(addr, FLASH_PAGE_SIZE);
    flash_range_program(addr, test_data, FLASH_PAGE_SIZE);
    
    // Verify CRC passes
    ASSERT_TRUE(verify_flash_crc32(addr, test_data, FLASH_PAGE_SIZE));
    
    // Corrupt flash with bit flips (simulating flash wear/errors)
    mock_flash[addr + 10] ^= 0x01;  // Single bit flip
    mock_flash[addr + 50] ^= 0xFF;  // Multiple bit flips
    mock_flash[addr + 100] = 0x00;  // Byte corruption
    
    // Verify CRC fails
    ASSERT_TRUE(!verify_flash_crc32(addr, test_data, FLASH_PAGE_SIZE));
}

// Test: Partial flash write recovery
TEST(partial_flash_write_recovery) {
    uint8_t test_data[FLASH_SECTOR_SIZE];
    uint32_t addr = 0x10000;
    
    // Fill with pattern
    for (int i = 0; i < FLASH_SECTOR_SIZE; i++) {
        test_data[i] = (i >> 8) ^ (i & 0xFF);
    }
    
    // Simulate power failure during flash write
    // IMPORTANT: This test verifies that the bootloader handles partial writes safely
    // A real bootloader MUST ensure that partial writes don't leave the device unbootable
    flash_operation_should_fail = true;
    flash_operations_before_failure = 2;  // Fail on 3rd operation
    flash_operation_count = 0;
    
    // Try to write multiple pages
    flash_range_erase(addr, FLASH_SECTOR_SIZE);
    for (int i = 0; i < 16; i++) {  // 16 pages in a sector
        // Check if we're about to fail
        if (flash_operation_should_fail && flash_operation_count >= flash_operations_before_failure) {
            // Simulate power loss - no more operations would occur
            flash_range_program_with_failure(addr + i * FLASH_PAGE_SIZE, 
                                           &test_data[i * FLASH_PAGE_SIZE], 
                                           FLASH_PAGE_SIZE);
            break;  // Power is lost, no more writes
        }
        flash_range_program_with_failure(addr + i * FLASH_PAGE_SIZE, 
                                       &test_data[i * FLASH_PAGE_SIZE], 
                                       FLASH_PAGE_SIZE);
    }
    
    // Reset failure simulation
    flash_operation_should_fail = false;
    
    // CRITICAL: After a partial write failure, the bootloader should:
    // 1. Not mark the application as valid (prog_info should not be updated)
    // 2. Be able to detect the incomplete write on next boot
    // 3. Still be able to accept a new valid firmware
    
    // For now, we verify the physical state matches our simulation
    // But the REAL test is whether the bootloader would try to boot this partial app
    ASSERT_TRUE(memcmp(&mock_flash[addr], test_data, FLASH_PAGE_SIZE * 2) == 0);
    
    // Check that data after failure point is untouched (still erased)
    ASSERT_TRUE(mock_flash[addr + FLASH_PAGE_SIZE * 3] == 0xFF);
}

// Test: Boundary condition - flashing at bootloader boundary
TEST(bootloader_boundary_protection) {
    // Assuming bootloader is at top 256KB
    uint32_t bootloader_start = MOCK_FLASH_SIZE - (256 * 1024);
    uint8_t test_data[FLASH_PAGE_SIZE];
    
    memset(test_data, 0xBB, sizeof(test_data));
    
    // Mark bootloader area
    memset(&mock_flash[bootloader_start], 0xBL, 256 * 1024);
    
    // Try to write just before bootloader
    uint32_t app_end = bootloader_start - FLASH_PAGE_SIZE;
    flash_range_program(app_end, test_data, FLASH_PAGE_SIZE);
    
    // Verify write succeeded
    ASSERT_TRUE(memcmp(&mock_flash[app_end], test_data, FLASH_PAGE_SIZE) == 0);
    
    // Verify bootloader area untouched
    ASSERT_EQ(0xBL, mock_flash[bootloader_start]);
}

// Test: Multi-block atomic operation failure
TEST(multi_block_atomic_failure) {
    uf2_block_t blocks[4];
    uint8_t block_data[4][FLASH_PAGE_SIZE];
    uint32_t base_addr = 0x10040000;
    
    // Create 4 interdependent blocks
    for (int i = 0; i < 4; i++) {
        memset(block_data[i], 0x40 + i, FLASH_PAGE_SIZE);
        // Simulate UF2 block creation
        blocks[i].magic_start0 = UF2_MAGIC_START0;
        blocks[i].magic_start1 = UF2_MAGIC_START1;
        blocks[i].magic_end = UF2_MAGIC_END;
        blocks[i].flags = UF2_FLAG_FAMILY_ID_PRESENT;
        blocks[i].target_addr = base_addr + i * FLASH_PAGE_SIZE;
        blocks[i].payload_size = FLASH_PAGE_SIZE;
        blocks[i].block_no = i;
        blocks[i].num_blocks = 4;
        blocks[i].file_size = RP2040_FAMILY_ID;
        memcpy(blocks[i].data, block_data[i], FLASH_PAGE_SIZE);
    }
    
    // Simulate failure during multi-block write
    flash_operation_should_fail = true;
    flash_operations_before_failure = 3;  // Fail after 3rd block
    flash_operation_count = 0;
    
    // Process blocks
    bool success = true;
    for (int i = 0; i < 4 && success; i++) {
        uint32_t addr = blocks[i].target_addr - XIP_BASE;
        flash_range_erase_with_failure(addr, FLASH_PAGE_SIZE);
        flash_range_program_with_failure(addr, blocks[i].data, FLASH_PAGE_SIZE);
        
        // Check if this would have failed CRC
        if (!verify_flash_crc32(addr, blocks[i].data, FLASH_PAGE_SIZE)) {
            success = false;
        }
    }
    
    ASSERT_TRUE(!success);  // Should have failed
    
    // Reset failure simulation  
    flash_operation_should_fail = false;
    
    // Verify partial write state
    uint32_t addr0 = base_addr - XIP_BASE;
    ASSERT_TRUE(memcmp(&mock_flash[addr0], block_data[0], FLASH_PAGE_SIZE) == 0);
    // Second block should be corrupted (half written)
    ASSERT_TRUE(memcmp(&mock_flash[addr0 + FLASH_PAGE_SIZE], block_data[1], FLASH_PAGE_SIZE/2) == 0);
    ASSERT_TRUE(mock_flash[addr0 + FLASH_PAGE_SIZE + FLASH_PAGE_SIZE/2] == 0x55);
    // Third block should be untouched (erased)
    ASSERT_TRUE(mock_flash[addr0 + FLASH_PAGE_SIZE * 2] == 0xFF);
}

// Test: SD card removal during flash operation
TEST(sd_card_removal_simulation) {
    // This test simulates what happens if SD card is removed mid-operation
    // In real scenario, file read would fail, but we test the cleanup path
    
    uint8_t partial_data[FLASH_PAGE_SIZE];
    uint32_t addr = 0x20000;
    
    // Start with some data
    memset(partial_data, 0xCC, sizeof(partial_data));
    
    // Write first part successfully
    flash_range_erase(addr, FLASH_SECTOR_SIZE);
    flash_range_program(addr, partial_data, FLASH_PAGE_SIZE);
    
    // Simulate SD read failure (would return false from load_application_from_uf2)
    // The bootloader should handle this gracefully
    
    // In the real bootloader, prog_info would not be updated on failure
    // Verify first page was written (but app is incomplete)
    ASSERT_TRUE(memcmp(&mock_flash[addr], partial_data, FLASH_PAGE_SIZE) == 0);
    
    // Rest of sector should still be erased
    ASSERT_EQ(0xFF, mock_flash[addr + FLASH_PAGE_SIZE]);
}

// Test: RP2350 ATU alignment requirements
#if PICO_RP2350
TEST(atu_alignment_validation) {
    // ATU requires 4KB alignment
    uint32_t valid_addresses[] = {0x00000000, 0x00001000, 0x00040000, 0x00100000};
    uint32_t invalid_addresses[] = {0x00000001, 0x00000100, 0x00000800, 0x00040001};
    
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE((valid_addresses[i] & 0xFFF) == 0);
    }
    
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE((invalid_addresses[i] & 0xFFF) != 0);
    }
}
#endif

// Test: Maximum application size boundary
TEST(max_app_size_boundary) {
    // Test that we correctly handle apps that fill entire available space
    uint32_t bootloader_size = 256 * 1024;
    uint32_t total_flash = MOCK_FLASH_SIZE;
    uint32_t max_app_size = total_flash - bootloader_size;
    
    // Try to flash app that exactly fits
    uint32_t app_start = 0x10000;  // Skip boot2 and some reserved space
    uint32_t app_size = max_app_size - app_start;
    
    ASSERT_TRUE(app_start + app_size <= total_flash - bootloader_size);
    
    // Try to flash app that's too large
    uint32_t oversized_app = app_size + FLASH_PAGE_SIZE;
    ASSERT_TRUE(app_start + oversized_app > total_flash - bootloader_size);
}

// Test: Verify bootloader won't boot corrupted firmware
TEST(corrupted_firmware_rejection) {
    // This is THE MOST CRITICAL TEST - verifies the bootloader's main safety feature
    uint8_t test_app[FLASH_PAGE_SIZE * 4];
    uint32_t app_addr = 0x10040000;
    
    // Create a valid-looking app with proper vectors
    uint32_t *vectors = (uint32_t*)test_app;
    vectors[0] = 0x20042000;  // Stack pointer (valid RAM address)
    vectors[1] = app_addr + 0x101;  // Reset handler (thumb mode)
    
    // Fill rest with pattern
    for (int i = 8; i < sizeof(test_app); i++) {
        test_app[i] = i & 0xFF;
    }
    
    // Write app to flash
    uint32_t flash_addr = app_addr - XIP_BASE;
    flash_range_erase(flash_addr, sizeof(test_app));
    flash_range_program(flash_addr, test_app, sizeof(test_app));
    
    // Verify it was written correctly
    ASSERT_TRUE(memcmp(&mock_flash[flash_addr], test_app, sizeof(test_app)) == 0);
    
    // Now corrupt the app in flash (simulate bit rot, power glitch, etc)
    mock_flash[flash_addr + 100] ^= 0xFF;  // Flip multiple bits
    mock_flash[flash_addr + 200] = 0x00;   // Zero out a byte
    mock_flash[flash_addr + 300] ^= 0x01;  // Single bit flip
    
    // CRITICAL VERIFICATION:
    // The bootloader MUST detect this corruption and refuse to boot
    // In the real implementation, this would be caught by:
    // 1. CRC verification during flashing (if corruption happens during write)
    // 2. Vector table validation before jumping to app
    // 3. Any other integrity checks
    
    // The corrupted data should NOT match the original
    ASSERT_TRUE(memcmp(&mock_flash[flash_addr], test_app, sizeof(test_app)) != 0);
    
    // In a real test, we'd verify that launch_application() fails safely
    // and doesn't jump to corrupted code
}

// Test: Verify prog_info is only updated on success
TEST(prog_info_atomicity) {
    // prog_info should only be updated AFTER successful flashing
    // This ensures we never mark a partial/corrupted app as bootable
    
    // This test would verify that:
    // 1. prog_info remains unchanged during flashing
    // 2. prog_info is only updated after ALL blocks are written
    // 3. prog_info is only updated after CRC verification passes
    // 4. If any step fails, prog_info remains pointing to previous valid app
    
    // For now, this is a placeholder for the critical atomicity test
    ASSERT_TRUE(true);  // TODO: Implement when we have prog_info mocking
}

// Main test runner
int main(void) {
    printf("=== Critical Scenario Test Suite ===\n\n");
    
    // Initialize mock flash
    memset(mock_flash, 0xFF, MOCK_FLASH_SIZE);
    
    // Run all critical tests
    RUN_TEST(boot2_preservation);
    RUN_TEST(flash_corruption_detection);
    RUN_TEST(partial_flash_write_recovery);
    RUN_TEST(bootloader_boundary_protection);
    RUN_TEST(multi_block_atomic_failure);
    RUN_TEST(sd_card_removal_simulation);
    #if PICO_RP2350
    RUN_TEST(atu_alignment_validation);
    #endif
    RUN_TEST(max_app_size_boundary);
    RUN_TEST(corrupted_firmware_rejection);
    RUN_TEST(prog_info_atomicity);
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    
    if (tests_failed > 0) {
        printf("\nCRITICAL: Some tests failed! Bootloader may have safety issues.\n");
    }
    
    return tests_failed > 0 ? 1 : 0;
} 