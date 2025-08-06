#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "proginfo.h"

#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/flash.h"

#if PICO_RP2040

#define VECTOR_HOLE_OFFSET 0x110
#define VECTOR_HOLE_SIZE 0x1C

#elif PICO_RP2350

#define VECTOR_HOLE_OFFSET 0x20
#define VECTOR_HOLE_SIZE 0x0C

#endif

#define PICOCALC_PROGINFO_ADDR (XIP_BASE + VECTOR_HOLE_OFFSET)
#define PICOCALC_BLINFO_ADDR (SRAM_BASE + VECTOR_HOLE_OFFSET)

// this info is stashed in the vector table in a gap that is not used by the Cortex-M0+.
// Apps *shouldn't* be putting data in there but you never know...

static_assert(sizeof(struct bl_info_t) <= VECTOR_HOLE_SIZE,
              "Proginfo struct too large for vector table hole\n");

static struct bl_info_t *proginfo = (void *)PICOCALC_PROGINFO_ADDR;

bool bl_proginfo_valid(void) { return (proginfo->magic == PICOCALC_BL_MAGIC); }

#if 0
// This is only for use by the application, not the bootloader or ui
// here for documentation purposes only

// Get the size of the available application space on the flash device
// TODO: read the current partition size instead if running on an RP2350

size_t bl_proginfo_flash_size(void)
{
  if (bl_proginfo_valid())
  {
    return proginfo->flash_end;
  }

  return PICO_FLASH_SIZE_BYTES;
}
#endif

const char *bl_proginfo_filename()
{
#if PICO_RP2040
  // bl_info_t.filename is not necessarily null terminated
  // so there's a bit of faff here to handle it safely
  // alternatively can use %.20s to directly use the field with printf

  static char filename[sizeof(proginfo->filename) + 1];

  if (bl_proginfo_valid())
  {
    strlcpy(filename, proginfo->filename, sizeof(filename));
    return filename;
  }
#endif

  return NULL;
}

static inline struct bl_info_t *struct_from_buf(void *buffer, uintptr_t start_addr, size_t size)
{
  if (start_addr > PICOCALC_PROGINFO_ADDR ||
      start_addr + size < PICOCALC_PROGINFO_ADDR + sizeof(struct bl_info_t))
  {
    return false;
  }

  return (void *)((uintptr_t)buffer + PICOCALC_PROGINFO_ADDR - start_addr);
}

bool bl_proginfo_set(void *buffer, uintptr_t start_addr, size_t size, uintptr_t flash_end,
                     const char *filename)
{
  struct bl_info_t *progbuf = struct_from_buf(buffer, start_addr, size);

  if (!progbuf)
  {
    return false;
  }

#if PICO_RP2040
  char fnbuf[sizeof(progbuf->filename) + 1];
  memset(fnbuf, 0, sizeof(fnbuf));
  strlcpy(fnbuf, filename, sizeof(fnbuf));
  memcpy(progbuf->filename, fnbuf, sizeof(progbuf->filename));
#endif

  progbuf->flash_end = flash_end;
  progbuf->magic     = PICOCALC_BL_MAGIC;

  return true;
}

// Set all the bits in the proginfo struct to 1
// Can reflash the page to insert the proginfo at the end
void bl_proginfo_clear(void *buffer, uintptr_t start_addr, size_t size)
{
  struct bl_info_t *progbuf = struct_from_buf(buffer, start_addr, size);

  if (!progbuf)
  {
    return;
  }

  memset(progbuf, 0xFF, sizeof(struct bl_info_t));
}

static struct bl_info_t *blinfo = (void *)PICOCALC_BLINFO_ADDR;

void *bl_info_get_flash_end(void)
{
  if (blinfo->magic != PICOCALC_BL_MAGIC)
  {
    return NULL;
  }

  return (void *)blinfo->flash_end;
}

void bl_info_set_flash_end(void *flash_end)
{
  blinfo->flash_end = (uintptr_t)flash_end;
  blinfo->magic     = PICOCALC_BL_MAGIC;
}

uintptr_t bl_proginfo_page(void) { return (PICOCALC_PROGINFO_ADDR & -FLASH_PAGE_SIZE); }

#if PICO_RP2350

bool bl_app_partition_get_info(void *workarea_base, uint32_t workarea_size,
                               uintptr_t *partition_offset, uint32_t *partition_size)
{
  if (rom_load_partition_table(workarea_base, workarea_size, false) != BOOTROM_OK)
  {
    return false;
  }

  uint32_t partition_info[3];

  uint32_t partition_and_flags =
      PT_INFO_PARTITION_LOCATION_AND_FLAGS | PT_INFO_SINGLE_PARTITION | (0 << 24);

  if (rom_get_partition_table_info(partition_info, 3, partition_and_flags) < 0)
  {
    return false;
  }

  uint16_t first_sector_number =
      (partition_info[1] & PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_BITS) >>
      PICOBIN_PARTITION_LOCATION_FIRST_SECTOR_LSB;
  uint16_t last_sector_number = (partition_info[1] & PICOBIN_PARTITION_LOCATION_LAST_SECTOR_BITS) >>
                                PICOBIN_PARTITION_LOCATION_LAST_SECTOR_LSB;
  uintptr_t app_end_addr = (last_sector_number + 1) * FLASH_SECTOR_SIZE;

  *partition_offset = first_sector_number * FLASH_SECTOR_SIZE;
  *partition_size   = app_end_addr - *partition_offset;

  return true;
}

#include "hardware/structs/qmi.h"

void bl_remap_flash(uint32_t offset)
{
  uint32_t sector_count = offset >> 12;  // 4k sectors

  qmi_hw->atrans[0] = ((QMI_ATRANS0_SIZE_RESET - sector_count) << QMI_ATRANS0_SIZE_LSB) |
                      (sector_count << QMI_ATRANS0_BASE_LSB);
  rom_flash_flush_cache();
}

#endif
