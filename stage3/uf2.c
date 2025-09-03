// UF2 parser implementation
// Originally from https://github.com/muzkr/hachi
// Copyright (c) 2024 muzkr
// Modified for SD boot by pelrun 2025

#include <pico/bootrom.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdalign.h>

#include "boot/uf2.h"
#include "hardware/regs/addressmap.h"

#include "pff.h"

#include "debug.h"
#include "uf2.h"
#include "proginfo.h"

#define UF2_BLOCK_SIZE 256

typedef struct
{
  uint32_t prog_addr;
  uint32_t num_blks;
  uint32_t num_blks_read;
  uint32_t num_blks_written;
} prog_state_t;

static alignas(256) struct uf2_block _block_buf;
static alignas(4) prog_state_t s;

static bool check_generic_block(const struct uf2_block* b);
static bool check_1st_block(const struct uf2_block* b);
static bool check_block(const prog_state_t* s, const struct uf2_block* b);

#ifdef PICO_RP2040
extern void *__logical_binary_start;
#elif PICO_RP2350
extern uintptr_t app_start_offset;
#endif

bool load_application_from_uf2(const char* filename)
{
  unsigned int bytes_received;
  FRESULT fr;

  if (fr = pf_open(filename), fr != FR_OK)
  {
    DEBUG_PRINT("open %s fail: %d\n", filename, fr);
    return false;
  }

  struct uf2_block* b = &_block_buf;

  char status[80] = "";

  while (fr = pf_read(b, sizeof(struct uf2_block), &bytes_received), fr == FR_OK)
  {
    if (bytes_received != sizeof(struct uf2_block))
    {
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

      memcpy((void*)b->target_addr, b->data, b->payload_size);

      s.num_blks_written++;
    }
    else
    {
      if (!check_1st_block(b))
      {
        continue;
      }

      memcpy((void*)b->target_addr, b->data, b->payload_size);

      s.prog_addr = b->target_addr;
      s.num_blks  = b->num_blocks;
      s.num_blks_written++;
    }

    if (s.num_blks == s.num_blks_written)
    {
      break;
    }
  }

  DEBUG_PRINT("Blocks parsed: %d\n", s.num_blks_read);
  DEBUG_PRINT("Blocks to write: %d\n", s.num_blks);
  DEBUG_PRINT("Blocks written: %d\n", s.num_blks_written);

  // Empty program file or not for this platform
  if (s.num_blks == 0)
  {
    return false;
  }

  if (s.num_blks != s.num_blks_written)
  {
    DEBUG_PRINT("Incomplete flash?\n");
    return false;
  }

#if PICO_RP2040
  bl_info_set_flash_end(&__logical_binary_start);
#elif PICO_RP2350
  // Should we even bother? UI queries the partition info anyway
  bl_info_set_flash_end((void*)XIP_BASE + PICO_FLASH_SIZE_BYTES - app_start_offset); // 0x103fe000
#endif

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
    DEBUG_PRINT("Wrong platform\n");
    return false;
  }

  // FIXME: make sure to not overwrite our own memory
  // perhaps stick our globals in SCRATCH_Y?
  if (b->target_addr < SRAM_BASE || b->target_addr >= (SRAM_END - 0x2000))
  {
    DEBUG_PRINT("Out of bounds %x > %x >= %x\n", SRAM_BASE, b->target_addr, (SRAM_END - 0x2000));
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
    DEBUG_PRINT("No first block\n");
    return false;
  }

  if (b->target_addr + (0x100 * b->num_blocks) > (SRAM_END - 0x2000))
  {
    DEBUG_PRINT("Range exceeds SRAM\n");
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
  if (s->prog_addr + UF2_BLOCK_SIZE * s->num_blks_written != b->target_addr)
  {
    return false;
  }

  return true;
}
