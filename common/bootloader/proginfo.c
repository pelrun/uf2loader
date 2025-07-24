#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "proginfo.h"

#include "hardware/flash.h"

// Get the size of the available application space on the flash device
// TODO: read the current partition size instead if running on an RP2350

#if PICO_RP2040

#define PICOCALC_PROGINFO_ADDR (void *)0x10000110

// this info is stashed in the vector table in a gap that is not used by the Cortex-M0+.
// Apps *shouldn't* be putting data in there but you never know...

static_assert(sizeof(struct bl_prog_info_t) <= 0x1C,
              "Proginfo struct too large for vector table hole\n");

struct bl_prog_info_t *proginfo = PICOCALC_PROGINFO_ADDR;

bool bl_proginfo_valid(void) { return (proginfo->magic == PICOCALC_BL_MAGIC); }

size_t bl_proginfo_flash_size(void)
{
  if (bl_proginfo_valid())
  {
    return proginfo->flash_end;
  }

  return PICO_FLASH_SIZE_BYTES;
}

// bl_prog_info_t.filename is not necessarily null terminated
// so there's a bit of faff here to handle it safely

const char *bl_proginfo_filename()
{
  static char filename[sizeof(proginfo->filename)+1];
  memset(filename, 0, sizeof(filename));

  if (bl_proginfo_valid())
  {
    memcpy(filename, proginfo->filename, sizeof(proginfo->filename));
    return filename;
  }
  return NULL;
}

bool bl_proginfo_set(void *buffer, uintptr_t flash_end, char *filename)
{
  struct bl_prog_info_t *progbuf = buffer;

  char fnbuf[sizeof(progbuf->filename)+1];
  memset(fnbuf, 0, sizeof(fnbuf));
  strlcpy(fnbuf, filename, sizeof(fnbuf));
  memcpy(progbuf->filename, fnbuf, sizeof(progbuf->filename));

  progbuf->flash_end = flash_end;
  progbuf->magic = PICOCALC_BL_MAGIC;

  return true;
}

#endif
