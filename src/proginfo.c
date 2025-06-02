#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hardware/flash.h"

#include "debug.h"

#include "proginfo.h"

volatile prog_info_t _prog_info_record __attribute__((section(".sdboot.prog_info")))
__attribute__((aligned(4096))) = {0};

volatile prog_info_t const *get_prog_info(void) { return &_prog_info_record; }

static_assert(sizeof(prog_info_t) <= FLASH_PAGE_SIZE, "prog info record larger than expected");

void clear_prog_info(void)
{
  DEBUG_PRINT("\nErase prog info block\n");
  flash_range_erase((uintptr_t)&_prog_info_record - XIP_BASE, FLASH_SECTOR_SIZE);
}

void set_prog_info(uint32_t prog_addr, uint32_t prog_size, const char *filename)
{
  uint8_t a1[FLASH_PAGE_SIZE];
  {
    prog_info_t *p1 = (prog_info_t *)a1;
    p1->prog_addr = prog_addr;
    p1->size = prog_size;
    strlcpy(p1->filename, filename, 80);
  }

  flash_range_program((uintptr_t)&_prog_info_record - XIP_BASE, a1, FLASH_PAGE_SIZE);
}

bool check_prog_info(void)
{
  volatile prog_info_t const *prog_info = get_prog_info();

  if (prog_info->prog_addr < PROG_AREA_BEGIN || prog_info->prog_addr >= PROG_AREA_END)
  {
    DEBUG_PRINT("ERR: prog_addr: %x < %x < %x\n", PROG_AREA_BEGIN, prog_info->prog_addr,
                PROG_AREA_END);
    return false;
  }
  if (_prog_info_record.size == 0 || _prog_info_record.size > PROG_AREA_SIZE)
  {
    DEBUG_PRINT("ERR: prog_size: 0 < %x < %x\n", prog_info->size, PROG_AREA_SIZE);
    return false;
  }

  // We'll not examine the vector table..

  return true;
}
