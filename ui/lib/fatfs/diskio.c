/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"     /* Basic definitions of FatFs */
#include "diskio.h" /* Declarations FatFs MAI */

#include "sdmmc.h"

enum
{
  DEV_MMC,
  DEV_FLASH,
  DEV_USB,
  DEV_RAM,
};

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status(BYTE pdrv /* Physical drive number to identify the drive */
)
{
  switch (pdrv)
  {
    case DEV_MMC:
      return MMC_disk_ready() ? 0 : STA_NOINIT;
    default:
      return STA_NOINIT;
  }
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize(BYTE pdrv /* Physical drive number to identify the drive */
)
{
  switch (pdrv)
  {
    case DEV_MMC:
      return MMC_disk_initialize() ? 0 : STA_NOINIT;
    default:
      return STA_NOINIT;
  }
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read(BYTE pdrv,    /* Physical drive number to identify the drive */
                  BYTE *buff,   /* Data buffer to store read data */
                  LBA_t sector, /* Start sector in LBA */
                  UINT count    /* Number of sectors to read */
)
{
  switch (pdrv)
  {
    case DEV_MMC:
      if (!MMC_disk_ready()) return RES_NOTRDY;
      return MMC_disk_read(buff, sector, count) ? RES_OK : RES_ERROR;
    default:
      return RES_PARERR;
  }
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write(BYTE pdrv,        /* Physical drive number to identify the drive */
                   const BYTE *buff, /* Data to be written */
                   LBA_t sector,     /* Start sector in LBA */
                   UINT count        /* Number of sectors to write */
)
{
  switch (pdrv)
  {
    case DEV_MMC:
      if (!MMC_disk_ready()) return RES_NOTRDY;
      return MMC_disk_write(buff, sector, count) ? RES_OK : RES_ERROR;
    default:
      return RES_PARERR;
  }
}

#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl(BYTE pdrv, /* Physical drive number (0..) */
                   BYTE cmd,  /* Control code */
                   void *buff /* Buffer to send/receive control data */
)
{
  if (pdrv != DEV_MMC) return RES_PARERR;

  if (!MMC_disk_ready()) return RES_NOTRDY;

  switch (cmd)
  {
    case CTRL_SYNC: /* Make sure that no pending write process */
      return MMC_sync() ? RES_OK : RES_ERROR;

    case GET_SECTOR_COUNT: /* Get number of sectors on the disk (uint32_t) */
    {
      int32_t count = MMC_get_sector_count();
      if (count > 0) {
        *(int32_t*)buff = count;
        return RES_OK;
      }
    }
    case GET_BLOCK_SIZE: /* Get erase block size in unit of sector (uint32_t) */
      *(uint32_t *)buff = 128;
      return RES_OK;

    default:
      break;
  }

  return RES_ERROR;
}
