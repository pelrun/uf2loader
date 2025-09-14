/*
 * usb_msc.c
 *
 * USB Mass Storage Class implementation
 */

#include <string.h>

#include "tusb.h"
#include "usb_msc.h"
#include "text_directory_ui.h"

#include "sdmmc.h"

bool sd_card_inserted(void);

// Block devices and filesystems
static uint16_t msc_block_size = 512;

void usb_msc_init(void)
{
  // Initialize TinyUSB device stack
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};

  tusb_init(BOARD_TUD_RHPORT, &dev_init);
} /* usb_msc_init */

// --------------------------------------------------------------------
// USB MSC callbacks
// --------------------------------------------------------------------

static bool is_mounted = false;

// Invoked when device is mounted by the host
void tud_mount_cb(void)
{
  is_mounted = true;
}

// Invoked when device is unmounted by the host
// NOTE: not working on Pico (tinyusb #2478 and #2700) use tud_ready() instead
void tud_umount_cb(void)
{
  is_mounted = false;
}

// Invoked to determine max LUN
// Max logical unit number (0-based), returns 0 for single LUN
uint8_t tud_msc_get_maxlun_cb(void) { return 0; }

// SCSI Inquiry data
// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters
// respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t p_vendor_id[8], uint8_t p_product_id[16],
                        uint8_t p_product_rev[4])
{
  (void)lun;
  static const char vendor[8] = "PICO";
  static const char product[16] = "UF2LOADER_MSC";
  static const char revision[4] = "1.0 ";

  memcpy(p_vendor_id, vendor, sizeof(vendor));
  memcpy(p_product_id, product, sizeof(product));
  memcpy(p_product_rev, revision, sizeof(revision));
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine
// the disk size Application update block count and block size SCSI Read Capacity
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
  (void)lun;
  unsigned int count;

  if ((count = MMC_get_sector_count()) > 0)
  {
    *block_size = msc_block_size;
    *block_count = count;
  }
  else
  {
    *block_size = 0;
    *block_count = 0;
  }
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void)lun;
  (void)power_condition;

  if (load_eject)
  {
    if (start)
    {
      // load disk storage
    }
    else
    {
      // unload disk storage
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
// SCSI Read10: transfer data from SD to host
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer,
                          uint32_t bufsize)
{
  (void)lun;
  (void)offset;

  // Read data from the block device
  if (!MMC_disk_read(buffer, lba, bufsize / msc_block_size))
  {
    return -1;
  }

  return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun)
{
  (void)lun;

  return true;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
// SCSI Write10: receive data from host and program to SD
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer,
                           uint32_t bufsize)
{
  (void)lun;
  (void)offset;

  // Read data from the block device
  return MMC_disk_write(buffer, lba, bufsize / msc_block_size) ? (int32_t)bufsize : -1;
}

// Flush any pending writes (not needed for SD)
void tud_msc_write10_flush_cb(uint8_t lun) { (void)lun; }

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  bool inserted = sd_card_inserted();

  if (!inserted)
  {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
    return false;
  }
  return true;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks (MUST not be handled here)
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
  (void)buffer;
  (void)bufsize;

  switch (scsi_cmd[0])
  {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      return -1;
  }
}

// Returns true if MSC interface is currently mounted/connected to a host
bool usb_msc_is_mounted(void) { return is_mounted && tud_ready(); }

// Stops the USB Mass Storage Class interface
void usb_msc_stop(void) { tud_disconnect(); }
