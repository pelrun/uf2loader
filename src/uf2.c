// UF2 parser implementation
// Originally from https://github.com/muzkr/hachi
// Copyright (c) 2024 muzkr
// Modified for SD boot by pelrun 2025

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef TEST
#include "boot/uf2.h"
#include "hardware/flash.h"
#else
#include "testuf2.h"
#endif

#include "debug.h"
#include "proginfo.h"
#include "text_directory_ui.h"
#include "uf2.h"

#define BOOT2_SIZE 256

#ifdef DRY_RUN
#define FLASH_ERASE(a1, a2) printf("Erase %x-%x\n", (a1), (a2))
#define FLASH_PROG(a1, a2, a3) printf("Flash %d bytes to %x\n", (a3), XIP_BASE + (a1))
#else
#define FLASH_ERASE(a1, a2) flash_range_erase((a1), (a2))
#define FLASH_PROG(a1, a2, a3) flash_range_program((a1), (a2), (a3))
#endif

typedef struct
{
  uint32_t prog_addr;
  uint32_t num_blks;
  uint32_t num_blks_read;
  uint32_t num_blks_flashed;
} prog_state_t;

static uint8_t _block_buf[sizeof(struct uf2_block)];

static bool check_generic_block(const struct uf2_block* b);
static bool check_1st_block(const struct uf2_block* b);
static bool check_block(const prog_state_t* s, const struct uf2_block* b);
static bool check_EOT(const prog_state_t* s);

static inline int sector_index(uint32_t addr) { return (addr - XIP_BASE) / FLASH_SECTOR_SIZE; }
static inline int page_index(uint32_t addr)
{
  return ((addr - XIP_BASE) % FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE;
}

bool load_application_from_uf2(const char* filename)
{
  uint8_t* buf = _block_buf;

  volatile prog_info_t const* prog_info = get_prog_info();

  FILE* fp = fopen(filename, "rb");

  if (fp == NULL)
  {
    DEBUG_PRINT("open %s fail: %s\n", filename, strerror(errno));
    return false;
  }

  prog_state_t s = {0};

  struct uf2_block* b = (struct uf2_block*)buf;

  char status[80] = "";

  while (fread(buf, sizeof(struct uf2_block), 1, fp) > 0)
  {
    s.num_blks_read++;

    if (s.num_blks_flashed > 0)
    {
      // Not the first block -------

      if (!check_block(&s, b))
      {
        continue;
      }

      if (s.num_blks_flashed % 100 == 0)
      {
        snprintf(status, sizeof(status), "Loading %d/%d...", s.num_blks_flashed, s.num_blks);
        text_directory_ui_set_status(status);
      }

      FLASH_PROG(b->target_addr - XIP_BASE, b->data, FLASH_PAGE_SIZE);

      s.num_blks_flashed++;
    }
    else
    {
      // The 1st block ----

      // FIXME: complex uf2's could have multiple separate flash blocks, don't
      // assume only one

      if (!check_1st_block(b))
      {
        continue;
      }

      int first_sector = sector_index(b->target_addr);
      int last_sector = sector_index(b->target_addr + FLASH_PAGE_SIZE * b->num_blocks - 4);

      if (first_sector == 0)
      {
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
      }
      else
      {
        FLASH_ERASE(FLASH_SECTOR_SIZE * first_sector,
                    FLASH_SECTOR_SIZE * (last_sector - first_sector + 1));
        FLASH_PROG(b->target_addr - XIP_BASE, b->data, FLASH_PAGE_SIZE);
      }

      clear_prog_info();

      s.prog_addr = b->target_addr;
      s.num_blks = b->num_blocks;
      s.num_blks_flashed++;
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

  set_prog_info(s.prog_addr + BOOT2_SIZE, s.num_blks * FLASH_PAGE_SIZE, strrchr(filename, '/') + 1);

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

#if PICO_RP2040
  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && b->file_size != RP2040_FAMILY_ID)
#elif PICO_RP2350
  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && b->file_size != RP2350_ARM_NS_FAMILY_ID &&
      b->file_size != RP2350_ARM_S_FAMILY_ID)
#endif
  {
    DEBUG_PRINT("Not for this platform\n");
    return false;
  }

  if (b->flags & UF2_FLAG_FAMILY_ID_PRESENT && b->file_size == ABSOLUTE_FAMILY_ID &&
      b->block_no == 0 && b->num_blocks == 2 && b->target_addr == 0x10FFFF00)
  {
    // Skip RP2350-E10 workaround block
    DEBUG_PRINT("Skip RP2350-E10 dummy block\n");
    return false;
  }
  if (b->target_addr < PROG_AREA_BEGIN || b->target_addr >= PROG_AREA_END)
  {
    DEBUG_PRINT("Out of bounds %x > %x >= %x\n", PROG_AREA_BEGIN, b->target_addr, PROG_AREA_END);
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

  if (b->target_addr + FLASH_PAGE_SIZE * b->num_blocks > PROG_AREA_END)
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
  if (s->num_blks_flashed != b->block_no)
  {
    return false;
  }
  if (s->prog_addr + FLASH_PAGE_SIZE * s->num_blks_flashed != b->target_addr)
  {
    return false;
  }

  return true;
}

static bool check_EOT(const prog_state_t* s)
{
  if (s->num_blks != s->num_blks_flashed)
  {
    DEBUG_PRINT("Not all blocks were flashed?\n");
    return false;
  }

  return true;
}
