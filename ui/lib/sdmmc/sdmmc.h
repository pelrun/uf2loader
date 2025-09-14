#ifndef _SDMMC_H
#define _SDMMC_H

#include <stdint.h>
#include <stdbool.h>

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

bool MMC_disk_ready(void);

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

bool MMC_disk_initialize(void);

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

bool MMC_disk_read(uint8_t *buff, int sector, unsigned int count);

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

bool MMC_disk_write(const uint8_t *buff, int sector, unsigned int count);

#endif

bool MMC_sync(void);
uint32_t MMC_get_block_size(void);
int32_t MMC_get_sector_count(void);

#endif // _SDMMC_H