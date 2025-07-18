#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Define XIP_BASE for tests
#ifndef XIP_BASE
#define XIP_BASE 0x10000000
#endif

// Define PICO_BOOT_STAGE2_SIZE if not defined
#ifndef PICO_BOOT_STAGE2_SIZE
#define PICO_BOOT_STAGE2_SIZE 0x100
#endif

// Define FLASH_PAGE_SIZE if not defined
#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 256
#endif

// Define MOCK_FLASH_SIZE if not defined
#ifndef MOCK_FLASH_SIZE
#define MOCK_FLASH_SIZE (2 * 1024 * 1024)  // 2MB
#endif

// Declare mock flash memory and related functions
extern uint8_t mock_flash[MOCK_FLASH_SIZE];
void mock_flash_init(void);
void mock_flash_erase(uint32_t addr, size_t size);

// Test macros
#define TEST_PASSED 1
#define TEST_FAILED 0

// Test assertion macro
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            printf("TEST FAILED: %s:%d - %s\n", __FILE__, __LINE__, message); \
            return TEST_FAILED; \
        } \
    } while (0)

// Test function pointer type
typedef int (*test_func_t)(void);

// Test case structure
typedef struct {
    const char *name;
    test_func_t func;
} test_case_t;

// Test runner macro
#define RUN_TEST_CASES(test_cases) \
    do { \
        int total = 0; \
        int passed = 0; \
        for (size_t i = 0; i < sizeof(test_cases)/sizeof(test_case_t); i++) { \
            printf("Running test: %s... ", test_cases[i].name); \
            fflush(stdout); \
            int result = test_cases[i].func(); \
            if (result == TEST_PASSED) { \
                printf("PASSED\n"); \
                passed++; \
            } else { \
                printf("FAILED\n"); \
            } \
            total++; \
        } \
        printf("\nTest Summary: %d/%d tests passed\n", passed, total); \
    } while (0)

// Mock flash memory for testing
extern uint8_t mock_flash[];
#define MOCK_FLASH_SIZE (2 * 1024 * 1024)  // 2MB

// Mock flash operations
void mock_flash_init(void);
void mock_flash_erase(uint32_t addr, size_t size);
void mock_flash_program(uint32_t addr, const uint8_t *data, size_t size);

// Mock implementations of Pico SDK flash functions for testing
static inline void flash_range_erase(uint32_t addr, size_t size) {
    // Convert from XIP address to flash offset
    if (addr >= XIP_BASE) {
        addr -= XIP_BASE;
    }
    
    // Align to 4K sector boundary
    uint32_t aligned_addr = addr & ~(4095);
    uint32_t end_addr = addr + size;
    uint32_t aligned_end = (end_addr + 4095) & ~(4095);
    size_t aligned_size = aligned_end - aligned_addr;
    
    // Call mock erase
    mock_flash_erase(aligned_addr, aligned_size);
}

static inline void flash_range_program(uint32_t addr, const uint8_t *data, size_t size) {
    // Convert from XIP address to flash offset
    if (addr >= XIP_BASE) {
        addr -= XIP_BASE;
    }
    
    // Call mock program
    mock_flash_program(addr, data, size);
}

// Mock implementation of verify_flash_crc32 for testing
static inline bool verify_flash_crc32(uint32_t addr, const uint8_t *expected_data, size_t size) {
    // Convert from XIP address to flash offset
    if (addr >= XIP_BASE) {
        addr -= XIP_BASE;
    }
    
    // Simple memory comparison for testing
    return memcmp(&mock_flash[addr], expected_data, size) == 0;
}

#endif // TEST_COMMON_H
