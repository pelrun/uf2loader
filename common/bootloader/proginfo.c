#include <assert.h>
#include <hardware/regs/watchdog.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "proginfo.h"

#include "pico/bootrom.h"
#include "boot/picobin.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"

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

// make sure the 0th window's value can be safely used for all windows
static_assert(QMI_ATRANS0_BASE_LSB == QMI_ATRANS1_BASE_LSB, "ATRANS definitions no longer valid");
static_assert(QMI_ATRANS0_SIZE_LSB == QMI_ATRANS1_SIZE_LSB, "ATRANS definitions no longer valid");
static_assert(QMI_ATRANS0_SIZE_RESET == QMI_ATRANS1_SIZE_RESET,
              "ATRANS definitions no longer valid");

void bl_remap_flash(uint32_t offset, uint32_t size)
{
  uint32_t sector_offset = offset >> 12;  // 4k sectors

  unsigned int base_reset[] = {QMI_ATRANS0_BASE_RESET, QMI_ATRANS1_BASE_RESET,
                               QMI_ATRANS2_BASE_RESET, QMI_ATRANS3_BASE_RESET};

  int size_remaining = size>>12;

  for (int i = 0; i < 4; i++)
  {
    // Offset entire 16MB range
    qmi_hw->atrans[i] = ((base_reset[i] + sector_offset) << QMI_ATRANS0_BASE_RESET);
    if (size_remaining >= (int)QMI_ATRANS0_SIZE_RESET)
    {
      qmi_hw->atrans[i] |= (QMI_ATRANS0_SIZE_RESET << QMI_ATRANS0_SIZE_LSB);
    }
    else if (size_remaining > 0)
    {
      qmi_hw->atrans[i] |= (size_remaining << QMI_ATRANS0_SIZE_LSB);
    }
    size_remaining -= QMI_ATRANS0_SIZE_RESET;
  }

  rom_flash_flush_cache();
}

#endif

// stored in scratch registers, used to send commands to stage3 from ui
void bl_stage3_command(enum bootmode_e mode, uint32_t arg)
{
  watchdog_hw->scratch[1] = mode;
  watchdog_hw->scratch[2] = arg;
  watchdog_hw->scratch[0] = PICOCALC_BL_MAGIC;
}

bool bl_get_command(enum bootmode_e *mode, uint32_t *arg)
{
  if (watchdog_hw->scratch[0] == PICOCALC_BL_MAGIC)
  {
    watchdog_hw->scratch[0] = 0;  // don't repeat on a reboot
    *mode                   = watchdog_hw->scratch[1];
    *arg                    = watchdog_hw->scratch[2];
    return true;
  }
  return false;
}

#if 0
// These are only for use by the application, not the bootloader or ui
// here for documentation purposes only

size_t bl_proginfo_flash_size(void)
{
  if (bl_proginfo_valid())
  {
    return proginfo->flash_end;
  }

  return PICO_FLASH_SIZE_BYTES;
}

// lazier version

int flash_size(void)
{
#if PICO_RP2040
  return PICO_FLASH_SIZE_BYTES - 4 * FLASH_SECTOR_SIZE;
#else
  uint32_t offset = ((qmi_hw->atrans[0] & QMI_ATRANS0_BASE_BITS) >> QMI_ATRANS0_BASE_LSB) * FLASH_SECTOR_SIZE;
  return PICO_FLASH_SIZE_BYTES - offset;
#endif
}

#endif
