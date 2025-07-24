/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for Petit FatFs (C)ChaN, 2014      */
/*-----------------------------------------------------------------------*/

#include <string.h>

#include "diskio.h"
#include "sdcard.h"

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(void) { return sdcard_init() ? 0 : STA_NOINIT; }

/*-----------------------------------------------------------------------*/
/* Read Partial Sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp(BYTE* buff,   /* Pointer to the destination object */
                   DWORD sector, /* Sector number (LBA) */
                   UINT offset,  /* Offset in the sector */
                   UINT count    /* Byte count (bit15:destination) */
)
{
  DRESULT res;
  uint8_t buffer[512];

  if (sdcard_read_sector(sector, buffer))
  {
    memcpy(buff, buffer + offset, count);
    return RES_OK;
  }

  return RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Write Partial Sector                                                  */
/*-----------------------------------------------------------------------*/

DRESULT disk_writep(const BYTE* buff, /* Pointer to the data to be written, NULL:Initiate/Finalize
                                         write operation */
                    DWORD sc          /* Sector number (LBA) or Number of bytes to send */
)
{
  static uint8_t sector_buffer[512];
  static uint32_t sector_address;
  static int sector_position;

  DRESULT res = RES_NOTRDY;

  if (!buff)
  {
    if (sc)
    {
      // Initiate write process
      sector_position = 0;
      sector_address  = sc;
      memset(sector_buffer, 0, 512);
      res = RES_OK;
    }
    else
    {
      // Finalize write process
      res = sdcard_write_sector(sector_address, sector_buffer) ? RES_OK : RES_ERROR;
    }
  }
  else
  {
    memcpy(sector_buffer + sector_position, buff, sc);
    sector_position += sc;
    res = RES_OK;
  }

  return res;
}
