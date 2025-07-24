// UF2 parser implementation
// Originally from https://github.com/muzkr/hachi
// Copyright (c) 2024 muzkr
// Modified for SD boot by pelrun 2025

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "boot/uf2.h"
#include "hardware/flash.h"

#include "debug.h"
#include "proginfo.h"
#include "text_directory_ui.h"
#include "uf2.h"

#define BOOT2_SIZE 256

#ifdef DRY_RUN
#define FLASH_ERASE(a1, a2) printf("Erase %x-%x\n", (a1), (a2))
#define FLASH_PROG(a1, a2, a3) printf("Flash %d bytes to %x\n", (a3), XIP_BASE + (a1))
#endif

uintptr_t prog_area_end;

typedef struct
{
  uint32_t prog_addr;
  uint32_t num_blks;
  uint32_t num_blks_read;
  uint32_t num_blks_written;
} prog_state_t;

static uint8_t _block_buf[sizeof(struct uf2_block)] __attribute((aligned (256)));
static prog_state_t s __attribute((aligned (4)));

static bool check_generic_block(const struct uf2_block* b);
static bool check_1st_block(const struct uf2_block* b);
static bool check_block(const prog_state_t* s, const struct uf2_block* b);
static bool check_EOT(const prog_state_t* s);

static inline int sector_index(uint32_t addr) { return (addr - XIP_BASE) / FLASH_SECTOR_SIZE; }
static inline int page_index(uint32_t addr)
{
  return ((addr - XIP_BASE) % FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE;
}

#if PICO_RP2040

#ifndef DRY_RUN
#define FLASH_ERASE(a1, a2) flash_range_erase((a1), (a2))
#define FLASH_PROG(a1, a2, a3) flash_range_program((a1), (a2), (a3))
#endif

bool handle_boot_stage2(const struct uf2_block* b, int first_sector, int last_sector)
{
  if (first_sector > 0)
  {
    return false;
  }

  // Sector #0 -----

  // We reuse our boot_stage2 instead of the one from the uf2
  // Unfortunately we can't avoid erasing it
  DEBUG_PRINT("\nErasing/reprogramming boot_stage2\n");
#ifndef DRY_RUN
  uint8_t a1[BOOT2_SIZE];
  memcpy(a1, (void*)XIP_BASE, BOOT2_SIZE);
#endif

  FLASH_ERASE(FLASH_SECTOR_SIZE * first_sector,
              FLASH_SECTOR_SIZE * (last_sector - first_sector + 1));
  FLASH_PROG(0, a1, BOOT2_SIZE);

  if (b->target_addr != XIP_BASE)
  {
    FLASH_PROG(b->target_addr - XIP_BASE, b->data, FLASH_PAGE_SIZE);
  }

  return true;
}

bool get_bootloader_info(void)
{
  if (*(uint32_t*)0x20000110 != PICOCALC_BL_MAGIC)
  {
    return false;
  }

  prog_area_end = *(uintptr_t*)0x20000114;

  return true;
}

bool family_valid(uint32_t family_id)
{
  return family_id == RP2040_FAMILY_ID;
}

#elif PICO_RP2350

extern uint8_t workarea[4 * 1024];

bool get_bootloader_info(void)
{
  ???????
}

bool family_valid(uint32_t family_id)
{
  return family_id == RP2350_ARM_NS_FAMILY_ID || family_id == RP2350_ARM_S_FAMILY_ID;
}

void __no_inline_not_in_flash_func(flash_op)(uint32_t flags, uint32_t flash_offs, size_t count,
                                             const uint8_t* data)
{
#ifdef PICO_FLASH_SIZE_BYTES
  hard_assert(flash_offs + count <= PICO_FLASH_SIZE_BYTES);
#endif
  invalid_params_if(HARDWARE_FLASH, flash_offs & (FLASH_SECTOR_SIZE - 1));
  invalid_params_if(HARDWARE_FLASH, count & (FLASH_SECTOR_SIZE - 1));
  rom_connect_internal_flash_fn connect_internal_flash_func =
      (rom_connect_internal_flash_fn)rom_func_lookup_inline(ROM_FUNC_CONNECT_INTERNAL_FLASH);
  rom_flash_exit_xip_fn flash_exit_xip_func =
      (rom_flash_exit_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_EXIT_XIP);
  rom_flash_op_fn flash_op_func = (rom_flash_op_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_OP);
  rom_flash_flush_cache_fn flash_flush_cache_func =
      (rom_flash_flush_cache_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_FLUSH_CACHE);
  assert(connect_internal_flash_func && flash_exit_xip_func && flash_op_func &&
         flash_flush_cache_func);
  flash_init_boot2_copyout();
  // Commit any pending writes to external RAM, to avoid losing them in the subsequent flush:
  xip_cache_clean_all();

  flash_rp2350_qmi_save_state_t qmi_save;
  flash_rp2350_save_qmi_cs1(&qmi_save);

  // No flash accesses after this point
  __compiler_memory_barrier();

  connect_internal_flash_func();
  flash_exit_xip_func();
  flash_op_func(flags, flash_offs, count, data);
  flash_flush_cache_func();  // Note this is needed to remove CSn IO force as well as cache flushing
  flash_enable_xip_via_boot2();

  flash_rp2350_restore_qmi_cs1(&qmi_save);
}

#ifndef DRY_RUN
#define FLASH_ERASE(a1, a2)                                            \
  flash_op((CFLASH_OP_VALUE_ERASE << CFLASH_OP_LSB) |                  \
               (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) | \
               (CFLASH_ASPACE_VALUE_STORAGE << CFLASH_ASPACE_LSB),     \
           (a1), (a2), NULL);
#define FLASH_PROG(a1, a2, a3)                                         \
  flash_op((CFLASH_OP_VALUE_PROGRAM << CFLASH_OP_LSB) |                \
               (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB) | \
               (CFLASH_ASPACE_VALUE_STORAGE << CFLASH_ASPACE_LSB),     \
           (a1), (a2), (a3));
#endif

bool handle_boot_stage2(int first_sector, int last_sector)
{
  // we don't use a modified boot stage2 on RP2350
  return false;
}

#endif

// FIXME: hardfaults when compiled under -Os!
// why is gcc producing unaligned reads?
bool __attribute__((optimize("-O0"))) load_application_from_uf2(const char* filename)
{
  uint8_t* buf = _block_buf;

  if (!get_bootloader_info())
  {
    text_directory_ui_set_status("Invalid bootloader!");
    return false;
  }

  FILE* fp = fopen(filename, "rb");

  if (fp == NULL)
  {
    DEBUG_PRINT("open %s fail: %s\n", filename, strerror(errno));
    return false;
  }

  struct uf2_block* b = (struct uf2_block*)buf;

  char status[80] = "";

  while (fread(buf, sizeof(struct uf2_block), 1, fp) > 0)
  {
    s.num_blks_read++;

    if (s.num_blks_written > 0)
    {
      // Not the first block -------

      if (!check_block(&s, b))
      {
        continue;
      }

#if PICO_RP2040
      // FIXME: store and flash this block last
      if (b->target_addr == 0x10000100)
      {
        bl_proginfo_set(b->data + 0x10, prog_area_end, strrchr(filename, '/') + 1);
      }
#endif

      if (s.num_blks_written % 100 == 0)
      {
        snprintf(status, sizeof(status), "Loading %d/%d...", s.num_blks_written, s.num_blks);
        text_directory_ui_set_status(status);
      }

      FLASH_PROG(b->target_addr - XIP_BASE, b->data, FLASH_PAGE_SIZE);

      s.num_blks_written++;
    }
    else
    {
      // The 1st block

      if (!check_1st_block(b))
      {
        continue;
      }

      int first_sector = sector_index(b->target_addr);
      int last_sector = sector_index(b->target_addr + FLASH_PAGE_SIZE * b->num_blocks - 4);

      if (!handle_boot_stage2(b, first_sector, last_sector))
      {
        FLASH_ERASE(FLASH_SECTOR_SIZE * first_sector,
                    FLASH_SECTOR_SIZE * (last_sector - first_sector + 1));
        FLASH_PROG(b->target_addr - XIP_BASE, b->data, FLASH_PAGE_SIZE);
      }

      s.prog_addr = b->target_addr;
      s.num_blks = b->num_blocks;
      s.num_blks_written++;
    }
  }

  fclose(fp);

  DEBUG_PRINT("Number of blocks parsed: %d\n", s.num_blks_read);
  DEBUG_PRINT("Number of blocks to flash: %d\n", s.num_blks);
  DEBUG_PRINT("Number of blocks flashed: %d\n", s.num_blks_flashed);

  // Empty program file or not for this platform
  if (s.num_blks == 0)
  {
    return false;
  }

  if (!check_EOT(&s))
  {
    return false;
  }

  return true;
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

  // TODO: should we even be accepting ABSOLUTE_FAMILY_ID?
  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && b->file_size == ABSOLUTE_FAMILY_ID &&
      b->block_no == 0 && b->num_blocks == 2 && b->target_addr == 0x10FFFF00)
  {
    // Skip RP2350-E10 workaround block
    DEBUG_PRINT("Skip RP2350-E10 dummy block\n");
    return false;
  }

  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && !family_valid(b->file_size))
  {
    DEBUG_PRINT("Not for this platform\n");
    return false;
  }

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

  if (b->block_no != 0)
  {
    DEBUG_PRINT("First block is missing\n");
    return false;
  }

  if (b->target_addr + FLASH_PAGE_SIZE * b->num_blocks > prog_area_end)
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

  if (s->num_blks != b->num_blocks)
  {
    return false;
  }
  if (s->num_blks_written != b->block_no)
  {
    return false;
  }
  if (s->prog_addr + FLASH_PAGE_SIZE * s->num_blks_written != b->target_addr)
  {
    return false;
  }

  return true;
}

static bool check_EOT(const prog_state_t* s)
{
  if (s->num_blks != s->num_blks_written)
  {
    DEBUG_PRINT("Not all blocks were flashed?\n");
    return false;
  }

  return true;
}
