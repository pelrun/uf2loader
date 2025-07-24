#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PICOCALC_BL_MAGIC 0xe98cc638

bool bl_proginfo_valid(void);
size_t bl_proginfo_flash_size(void);
const char *bl_proginfo_filename(void);

#if PICO_RP2040

struct bl_prog_info_t
{
  uint32_t magic;
  uint32_t flash_end;
  char filename[20];
};

bool bl_proginfo_set(void* dest, uintptr_t flash_end, char *filename);

#endif
