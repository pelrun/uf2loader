#ifndef ATU_H
#define ATU_H

#include <stdint.h>
#include <stdbool.h>

#if PICO_RP2350

// ATU (Address Translation Unit) base address
#define ATU_BASE 0x4008F000u

// ATU window 4 control register offset
#define ATU_WINDOW4_CTL_OFFSET 0x40

// ATU window control bits
#define ATU_WINDOW_ENABLE (1u << 0)

/**
 * Configure ATU window 4 to map physical flash to virtual address 0x10000000
 * @param phys_offset Physical offset from flash base (must be 4KB aligned)
 */
static inline bool atu_window4_map(uint32_t phys_offset) {
    if (phys_offset & 0xFFFu) {
        // Offset must be 4 KB aligned
        return false;
    }
    // Window 4 control register:
    // - bits[23:4] = physical address offset >> 12
    // - virtual base is fixed at 0x10000000 for window 4
    // - bit 0 = enable
    volatile uint32_t *atu_window4_ctl = (volatile uint32_t *)(ATU_BASE + ATU_WINDOW4_CTL_OFFSET);
    
    *atu_window4_ctl = ((phys_offset >> 12) & 0xFFFFF) | // Physical address bits[23:4]
                       (0x10000000u) |                    // Virtual base address
                       ATU_WINDOW_ENABLE;                 // Enable bit
    return true;
}

/**
 * Initialize ATU for application slot remapping
 * Maps the application area (starting at 256KB offset) to virtual address 0x10000000
 */
static inline bool atu_init_app_remap(void) {
    // Map physical offset 0x40000 (256KB) to virtual 0x10000000
    return atu_window4_map(0x00040000);
}

#endif // PICO_RP2350

#endif // ATU_H 