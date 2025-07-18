#define DRY_RUN
#define XIP_BASE 0x10000000
#define PICO_FLASH_SIZE_BYTES 0x200000
#define PICO_RP2040 1

/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TEST_UF2_H
#define TEST_UF2_H

#include <stdint.h>
#include <assert.h>
#include <string.h>

/** \file uf2.h
*  \defgroup boot_uf2_headers boot_uf2_headers
*
* \brief Header file for the UF2 format supported by a RP2xxx chip in BOOTSEL mode
*/

// Include the common UF2 block structure definition
#include "uf2_test_utils.h"

#endif // TEST_UF2_H

