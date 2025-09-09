#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PICOCALC_BL_MAGIC 0xe98cc638

enum bootmode_e
{
  BOOT_DEFAULT,
  BOOT_SD,
  BOOT_UPDATE,
  BOOT_RAM,
};

struct bl_info_t
{
  uint32_t magic;
  uint32_t flash_end;
#if PICO_RP2040
  char filename[20];  // RP2040 only!
#endif
};

// These read directly from the flash
bool bl_proginfo_valid(void);
const char *bl_proginfo_filename(void);

// These modify a buffer prior to flashing
bool bl_proginfo_set(void* buffer, uintptr_t start_addr, size_t size, uintptr_t flash_end, const char *filename);
void bl_proginfo_clear(void* buffer, uintptr_t start_addr, size_t size);
// Address of the start of the page containing the proginfo
uintptr_t bl_proginfo_page(void);

// This is always in RAM
void *bl_info_get_flash_end(void);
void bl_info_set_flash_end(void *flash_end);

#if PICO_RP2350
bool bl_app_partition_get_info(void *workarea_base, uint32_t workarea_size, uintptr_t *app_start_offset, uint32_t *app_size);
void bl_remap_flash(uint32_t offset, uint32_t size);
#endif

// stored in scratch registers, used to send commands to stage3 from ui
void bl_stage3_command(enum bootmode_e mode, uint32_t arg);
bool bl_get_command(enum bootmode_e *mode, uint32_t *arg);
