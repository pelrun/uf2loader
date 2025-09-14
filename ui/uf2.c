// UF2 parser implementation
// Originally from https://github.com/muzkr/hachi
// Copyright (c) 2024 muzkr
// Modified for SD boot by pelrun 2025

#include <pico.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "boot/uf2.h"
#include "hardware/flash.h"
#include "pico/bootrom.h"

#include "debug.h"
#include "proginfo.h"
#include "text_directory_ui.h"
#include "uf2.h"
#include "ff.h"
#include "ui.h"

uintptr_t prog_area_end;

typedef struct
{
  const char* filename;
  uint32_t prog_addr;
  uint32_t num_blks;
  uint32_t num_blks_read;
  uint32_t num_blks_written;
  uint32_t family_id;
  bool malformed_uf2;  // indicates a badly formed pico2 uf2
} prog_state_t;

static struct uf2_block _block_buf __attribute((aligned(256)));
static prog_state_t s __attribute((aligned(4)));

static bool check_generic_block(const struct uf2_block* b);
static bool check_1st_block(const struct uf2_block* b);
static bool check_block(const prog_state_t* s, const struct uf2_block* b);

#if PICO_RP2040

#define BOOT2_SIZE 256

static inline int FLASH_ERASE(uintptr_t address, uint32_t size_bytes)
{
  flash_range_erase(address - XIP_BASE, size_bytes);
  return 0;
}

static inline int FLASH_PROG(uintptr_t address, const void* buf, uint32_t size_bytes)
{
  flash_range_program(address - XIP_BASE, buf, size_bytes);
  return 0;
}

bool handle_boot_stage2(const struct uf2_block* b)
{
  if (b->target_addr >= XIP_BASE + FLASH_SECTOR_SIZE)
  {
    return false;
  }

  // Sector #0 -----

  // We reuse our boot_stage2 instead of the one from the uf2
  // Unfortunately we can't avoid erasing it
  DEBUG_PRINT("\nErasing/reprogramming boot_stage2\n");
  uint8_t boot2[BOOT2_SIZE];
  memcpy(boot2, (void*)XIP_BASE, BOOT2_SIZE);

  FLASH_ERASE(XIP_BASE, b->num_blocks * b->payload_size);
  FLASH_PROG(XIP_BASE, boot2, BOOT2_SIZE);

  if (b->target_addr != XIP_BASE)
  {
    FLASH_PROG(b->target_addr, b->data, FLASH_PAGE_SIZE);
  }

  return true;
}

bool family_valid(uint32_t family_id) { return family_id == RP2040_FAMILY_ID; }

#elif PICO_RP2350

bool family_valid(uint32_t family_id)
{
  return family_id == RP2350_ARM_NS_FAMILY_ID || family_id == RP2350_ARM_S_FAMILY_ID ||
         family_id == RP2350_RISCV_FAMILY_ID;
}

static inline int FLASH_ERASE(uintptr_t address, uint32_t size_bytes)
{
  cflash_flags_t cflash_flags = {(CFLASH_OP_VALUE_ERASE << CFLASH_OP_LSB) |
                                 (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
                                 (CFLASH_ASPACE_VALUE_RUNTIME << CFLASH_ASPACE_LSB)};

  // Round up size_bytes or rom_flash_op will throw an alignment error
  uint32_t size_aligned = (size_bytes + 0x1FFF) & -FLASH_SECTOR_SIZE;

  return rom_flash_op(cflash_flags, address, size_aligned, NULL);
}

static inline int FLASH_PROG(uintptr_t address, const void* buf, uint32_t size_bytes)
{
  cflash_flags_t cflash_flags = {(CFLASH_OP_VALUE_PROGRAM << CFLASH_OP_LSB) |
                                 (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) |
                                 (CFLASH_ASPACE_VALUE_RUNTIME << CFLASH_ASPACE_LSB)};

  return rom_flash_op(cflash_flags, address, size_bytes, (void*)buf);
}

// we don't use a modified boot stage2 on RP2350
bool handle_boot_stage2(const struct uf2_block* b) { return false; }

#endif

char* get_short_path(const char* path)
{
  static char sfn_path[512];

  // walk the path and get the SFN version of each part
  char str[strlen(path)+1];
  char* token;
  char* rest = str;

  strlcpy(str, path, sizeof(str));
  sfn_path[0] = 0;

  f_chdir("/");

  while ((token = strtok_r(rest, "/", &rest)))
  {
    FILINFO fno;

    if ((f_stat(token, &fno) != FR_OK) || ((fno.fattrib & AM_DIR) && (f_chdir(token) != FR_OK)))
    {
      f_chdir("/");
      return NULL;
    }

    strcat(sfn_path, "/");
    strcat(sfn_path, fno.altname);
  }

  f_chdir("/");
  return sfn_path;
}

// FIXME: hardfaults when compiled under -Os!
// why is gcc producing unaligned reads?
enum uf2_result_e __attribute__((optimize("-O0"))) load_application_from_uf2(const char* filename)
{
  FIL fp;
  FRESULT res;
  unsigned int bytes_read;

  prog_area_end = (uintptr_t)bl_info_get_flash_end();
  if (!prog_area_end)
  {
    text_directory_ui_set_status("Invalid bootloader!");
    return UF2_UNKNOWN;
  }

  // explicitly clear state in case a previous attempt failed
  memset(&s, 0, sizeof(s));

  s.filename = filename;

  res = f_open(&fp, filename, FA_READ);

  if (res != FR_OK)
  {
    DEBUG_PRINT("open %s fail: %s\n", filename, res);
    return UF2_UNKNOWN;
  }

  struct uf2_block* b = &_block_buf;

  char status[80] = "";

  while (f_read(&fp, b, sizeof(struct uf2_block), &bytes_read) == FR_OK)
  {
    if (bytes_read < sizeof(struct uf2_block))
    {
      // Hit EOF
      break;
    }

    s.num_blks_read++;

    if (s.num_blks_written > 0)
    {
      // Not the first block -------

      if (!check_block(&s, b))
      {
        continue;
      }

      if (s.num_blks_written % 100 == 0)
      {
        snprintf(status, sizeof(status), "Loading %d/%d...", s.num_blks_written, s.num_blks);
        text_directory_ui_set_status(status);
      }

      // if block contains proginfo area, set it to all 0xFF's
      bl_proginfo_clear(b->data, b->target_addr, b->payload_size);

      FLASH_PROG(b->target_addr, b->data, FLASH_PAGE_SIZE);

      s.num_blks_written++;
    }
    else
    {
      // The 1st block
      s.num_blks = b->num_blocks - (s.malformed_uf2 ? 1 : 0);

      if (!check_1st_block(b))
      {
        continue;
      }

      text_directory_ui_set_status("Erasing flash...");
      if (!handle_boot_stage2(b))
      {
        if (FLASH_ERASE(b->target_addr, s.num_blks * b->payload_size) < 0)
        {
          // Don't even attempt to program if the erase fails
          return UF2_UNKNOWN;
        }

        FLASH_PROG(b->target_addr, b->data, FLASH_PAGE_SIZE);
      }

      s.prog_addr = b->target_addr;
      s.num_blks_written++;
    }
  }

  f_close(&fp);

  DEBUG_PRINT("Number of blocks parsed: %d\n", s.num_blks_read);
  DEBUG_PRINT("Number of blocks to flash: %d\n", s.num_blks);
  DEBUG_PRINT("Number of blocks flashed: %d\n", s.num_blks_written);

  if (s.num_blks == 0)
  {
    return UF2_BAD;
  }

  if (s.num_blks_written == 0)
  {
    return UF2_WRONG_PLATFORM;
  }

  if (s.num_blks != s.num_blks_written)
  {
    return UF2_BAD;
  }

#if PICO_RP2040
  // reflash the page containing the proginfo
  // Since we're only clearing bits, erase is unnecessary
  uint8_t page_copy[FLASH_PAGE_SIZE];
  memcpy(page_copy, (void*)bl_proginfo_page(), FLASH_PAGE_SIZE);

  bl_proginfo_set(page_copy, bl_proginfo_page(), FLASH_PAGE_SIZE, prog_area_end, filename);
  FLASH_PROG(bl_proginfo_page(), page_copy, FLASH_PAGE_SIZE);
#endif

  return UF2_LOADED;
}

static bool check_generic_block(const struct uf2_block* b)
{
  if (UF2_MAGIC_START0 != b->magic_start0 || UF2_MAGIC_START1 != b->magic_start1  //
      || UF2_MAGIC_END != b->magic_end)
  {
    DEBUG_PRINT("Invalid UF2 magic\n");
    return false;
  }
  if (b->flags & UF2_FLAG_NOT_MAIN_FLASH)
  {
    DEBUG_PRINT("Not for flashing\n");
    return false;
  }
  if (b->target_addr % FLASH_PAGE_SIZE)
  {
    DEBUG_PRINT("Bad alignment\n");
    return false;
  }
  if (b->payload_size != FLASH_PAGE_SIZE)
  {
    DEBUG_PRINT("Incorrect block size\n");
    return false;
  }
  if (b->num_blocks == 0)
  {
    DEBUG_PRINT("Nothing to write\n");
    return false;
  }
  if (b->block_no >= b->num_blocks)
  {
    DEBUG_PRINT("Block count exceeded\n");
    return false;
  }

  // *yes* b->file_size is actually family_id when the UF2_FLAG_FAMILY_ID_PRESENT flag is set

  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && b->file_size == ABSOLUTE_FAMILY_ID &&
      b->block_no == 0 && b->target_addr == 0x10FFFF00)
  {
    // Skip RP2350-E10 workaround block
    DEBUG_PRINT("Skip RP2350-E10 dummy block\n");

    if (b->num_blocks != 2)
    {
      // need to deal with a malformed uf2
      s.malformed_uf2 = true;
    }

    return false;
  }

  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && !family_valid(b->file_size))
  {
    DEBUG_PRINT("Not for this platform\n");
    s.family_id = b->file_size;
    return false;
  }
#if ENABLE_RAM_APPS
  if (b->target_addr >= SRAM_BASE && b->target_addr < SRAM_END)
  {
    // This is a no-flash binary, so we're going to immediately reboot into the bootloader to run it
    // convert path completely to short filename as bootloader can't use LFN

    char* sfn_path = get_short_path(s.filename);
    if (sfn_path)
    {
      text_directory_ui_set_status("Launching RAM-only app");
      bl_stage3_command(BOOT_RAM, (uintptr_t)sfn_path);
      reboot();
    }
    return false;
  }
#endif

  if (b->target_addr < XIP_BASE || b->target_addr >= prog_area_end)
  {
    DEBUG_PRINT("Out of bounds %x > %x >= %x\n", XIP_BASE, b->target_addr, prog_area_end);
    return false;
  }

  return true;
}

static bool check_1st_block(const struct uf2_block* b)
{
  if (!check_generic_block(b))
  {
    return false;
  }

  if (b->block_no != (s.malformed_uf2 ? 1 : 0))
  {
    DEBUG_PRINT("First block is missing\n");
    return false;
  }

  if (b->target_addr + FLASH_PAGE_SIZE * s.num_blks > prog_area_end)
  {
    DEBUG_PRINT("Requested range exceeds flash area\n");
    return false;
  }

  return true;
}

static bool check_block(const prog_state_t* s, const struct uf2_block* b)
{
  if (!check_generic_block(b))
  {
    return false;
  }

  if (s->num_blks != b->num_blocks - (s->malformed_uf2 ? 1 : 0))
  {
    return false;
  }
  if (s->num_blks_written != b->block_no - (s->malformed_uf2 ? 1 : 0))
  {
    return false;
  }
  if (s->prog_addr + FLASH_PAGE_SIZE * s->num_blks_written != b->target_addr)
  {
    return false;
  }

  return true;
}
