#ifndef BL_DETECT_H
#define BL_DETECT_H

#include <stdint.h>

// Function declarations
uint32_t GetFlashSize(void);
uint32_t GetAppSize(void);

// Flash functions
#ifndef TEST
#include "hardware/flash.h"
#else
// For testing, provide a mock implementation
uint32_t flash_get_size(void);
#endif

// Constants
#define PICOCALC_BL_MAGIC 0xe98cc638

#endif // BL_DETECT_H
