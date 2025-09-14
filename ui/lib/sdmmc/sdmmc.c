/*------------------------------------------------------------------------/
/  Foolproof MMCv3/SDv1/SDv2 (in SPI mode) control module
/-------------------------------------------------------------------------/
/
/  Copyright (C) 2019, ChaN, all right reserved.
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------/
  Features and Limitations:

  * Easy to Port Bit-banging SPI
    It uses only four GPIO pins. No complex peripheral needs to be used.

  * Platform Independent
    You need to modify only a few macros to control the GPIO port.

  * Low Speed
    The data transfer rate will be several times slower than hardware SPI.

  * No Media Change Detection
    Application program needs to perform a f_mount() after media change.

/-------------------------------------------------------------------------*/

// modified for Raspberry Pi Pico by KenKen (Use hardware SPI)
// further modified for petit fatfs by pelrun

// #include "ff.h"     /* Basic definitions of FatFs */
// #include "diskio.h" /* Declarations FatFs MAI */

/*-------------------------------------------------------------------------*/
/* Platform dependent macros and functions needed to be modified           */
/*-------------------------------------------------------------------------*/

#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

#include "sdmmc.h"

#define SD_SPICH spi0
#define SD_SPI_MISO 16
#define SD_SPI_CS 17
#define SD_SPI_SCK 18
#define SD_SPI_MOSI 19

#define SD_SPI_BAUDRATE_INIT (300 * 1000)
#define SD_SPI_BAUDRATE (20 * 1000 * 1000)

#define CS_H() gpio_put(SD_SPI_CS, 1) /* Set MMC CS "high" */
#define CS_L() gpio_put(SD_SPI_CS, 0) /* Set MMC CS "low" */

/*--------------------------------------------------------------------------

   Module Private Functions

---------------------------------------------------------------------------*/

/* MMC/SD command (SPI mode) */
#define CMD0 (0)           /* GO_IDLE_STATE */
#define CMD1 (1)           /* SEND_OP_COND */
#define ACMD41 (0x80 + 41) /* SEND_OP_COND (SDC) */
#define CMD8 (8)           /* SEND_IF_COND */
#define CMD9 (9)           /* SEND_CSD */
#define CMD10 (10)         /* SEND_CID */
#define CMD12 (12)         /* STOP_TRANSMISSION */
#define CMD13 (13)         /* SEND_STATUS */
#define ACMD13 (0x80 + 13) /* SD_STATUS (SDC) */
#define CMD16 (16)         /* SET_BLOCKLEN */
#define CMD17 (17)         /* READ_SINGLE_BLOCK */
#define CMD18 (18)         /* READ_MULTIPLE_BLOCK */
#define CMD23 (23)         /* SET_BLOCK_COUNT */
#define ACMD23 (0x80 + 23) /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24 (24)         /* WRITE_BLOCK */
#define CMD25 (25)         /* WRITE_MULTIPLE_BLOCK */
#define CMD32 (32)         /* ERASE_ER_BLK_START */
#define CMD33 (33)         /* ERASE_ER_BLK_END */
#define CMD38 (38)         /* ERASE */
#define CMD55 (55)         /* APP_CMD */
#define CMD58 (58)         /* READ_OCR */

/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC3 0x01  /* MMC ver 3 */
#define CT_MMC4 0x02  /* MMC ver 4+ */
#define CT_MMC 0x03   /* MMC */
#define CT_SDC1 0x04  /* SD ver 1 */
#define CT_SDC2 0x08  /* SD ver 2+ */
#define CT_SDC 0x0C   /* SD */
#define CT_BLOCK 0x10 /* Block addressing */

static bool Status = false; /* Disk status */

static uint8_t CardType; /* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */

/*-----------------------------------------------------------------------*/
/* Transmit bytes to the card                                            */
/*-----------------------------------------------------------------------*/

static void xmit_mmc(const uint8_t *buff, /* Data to be sent */
                     unsigned int bc      /* Number of bytes to send */
)
{
  spi_write_blocking(SD_SPICH, buff, bc);
}

/*-----------------------------------------------------------------------*/
/* Receive bytes from the card                                           */
/*-----------------------------------------------------------------------*/

static void rcvr_mmc(uint8_t *buff,  /* Pointer to read buffer */
                     unsigned int bc /* Number of bytes to receive */
)
{
  spi_read_blocking(SD_SPICH, 0xFF, buff, bc);
}

/*-----------------------------------------------------------------------*/
/* Wait for card ready                                                   */
/*-----------------------------------------------------------------------*/

static int wait_ready(void) /* 1:OK, 0:Timeout */
{
  uint8_t d;
  unsigned int tmr;

  for (tmr = 5000; tmr; tmr--)
  { /* Wait for ready in timeout of 500ms */
    rcvr_mmc(&d, 1);
    if (d == 0xFF) break;
    sleep_us(100);
  }

  return tmr ? 1 : 0;
}

/*-----------------------------------------------------------------------*/
/* Deselect the card and release SPI bus                                 */
/*-----------------------------------------------------------------------*/

static void deselect(void)
{
  uint8_t d;

  CS_H();          /* Set CS# high */
  rcvr_mmc(&d, 1); /* Dummy clock (force DO hi-z for multiple slave SPI) */
}

/*-----------------------------------------------------------------------*/
/* Select the card and wait for ready                                    */
/*-----------------------------------------------------------------------*/

static int select(void) /* 1:OK, 0:Timeout */
{
  uint8_t d;

  CS_L();                     /* Set CS# low */
  rcvr_mmc(&d, 1);            /* Dummy clock (force DO enabled) */
  if (wait_ready()) return 1; /* Wait for card ready */

  deselect();
  return 0; /* Failed */
}

/*-----------------------------------------------------------------------*/
/* Receive a data packet from the card                                   */
/*-----------------------------------------------------------------------*/

static int rcvr_datablock(                 /* 1:OK, 0:Failed */
                          uint8_t *buff,   /* Data buffer to store received data */
                          unsigned int btr /* Byte count */
)
{
  uint8_t d[2];

  for (int tmr = 1000; tmr; tmr--)
  { /* Wait for data packet in timeout of 100ms */
    rcvr_mmc(d, 1);
    if (d[0] != 0xFF) break;
    sleep_us(100);
  }
  if (d[0] != 0xFE) return 0; /* If not valid data token, return with error */

  rcvr_mmc(buff, btr); /* Receive the data block into buffer */
  rcvr_mmc(d, 2);      /* Discard CRC */

  return 1; /* Return with success */
}

/*-----------------------------------------------------------------------*/
/* Send a data packet to the card                                        */
/*-----------------------------------------------------------------------*/

static int xmit_datablock(                     /* 1:OK, 0:Failed */
                          const uint8_t *buff, /* 512 byte data block to be transmitted */
                          uint8_t token        /* Data/Stop token */
)
{
  uint8_t d[2];

  if (!wait_ready()) return 0;

  d[0] = token;
  xmit_mmc(d, 1); /* Xmit a token */
  if (token != 0xFD)
  {                            /* Is it data token? */
    xmit_mmc(buff, 512);       /* Xmit the 512 byte data block to MMC */
    rcvr_mmc(d, 2);            /* Xmit dummy CRC (0xFF,0xFF) */
    rcvr_mmc(d, 1);            /* Receive data response */
    if ((d[0] & 0x1F) != 0x05) /* If not accepted, return with error */
      return 0;
  }

  return 1;
}

/*-----------------------------------------------------------------------*/
/* Send a command packet to the card                                     */
/*-----------------------------------------------------------------------*/

static uint8_t send_cmd(             /* Returns command response (bit7==1:Send failed)*/
                        uint8_t cmd, /* Command byte */
                        uint32_t arg /* Argument */
)
{
  uint8_t n, d, buf[6];

  if (cmd & 0x80)
  { /* ACMD<n> is the command sequense of CMD55-CMD<n> */
    cmd &= 0x7F;
    n = send_cmd(CMD55, 0);
    if (n > 1) return n;
  }

  /* Select the card and wait for ready except to stop multiple block read */
  if (cmd != CMD12)
  {
    deselect();
    if (!select()) return 0xFF;
  }

  /* Send a command packet */
  buf[0] = 0x40 | cmd;           /* Start + Command index */
  buf[1] = (uint8_t)(arg >> 24); /* Argument[31..24] */
  buf[2] = (uint8_t)(arg >> 16); /* Argument[23..16] */
  buf[3] = (uint8_t)(arg >> 8);  /* Argument[15..8] */
  buf[4] = (uint8_t)arg;         /* Argument[7..0] */
  n = 0x01;                      /* Dummy CRC + Stop */
  if (cmd == CMD0) n = 0x95;     /* (valid CRC for CMD0(0)) */
  if (cmd == CMD8) n = 0x87;     /* (valid CRC for CMD8(0x1AA)) */
  buf[5] = n;
  xmit_mmc(buf, 6);

  /* Receive command response */
  if (cmd == CMD12) rcvr_mmc(&d, 1); /* Skip a stuff byte when stop reading */
  n = 10;                            /* Wait for a valid response in timeout of 10 attempts */
  do rcvr_mmc(&d, 1);
  while ((d & 0x80) && --n);

  return d; /* Return with the response value */
}

/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

bool MMC_disk_ready() { return Status; }

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/

bool MMC_disk_initialize(void)
{
  uint8_t n, ty, cmd, buf[4];
  unsigned int tmr;

  sleep_us(10000); /* 10ms */

  // Enable SPI at 300khz and connect to GPIOs
  spi_init(SD_SPICH, SD_SPI_BAUDRATE_INIT);
  gpio_set_function(SD_SPI_MISO, GPIO_FUNC_SPI);
  gpio_set_function(SD_SPI_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(SD_SPI_SCK, GPIO_FUNC_SPI);
  gpio_set_pulls(SD_SPI_MISO, true, false);  // pull-up DO
  gpio_init(SD_SPI_CS);
  CS_H();
  gpio_set_dir(SD_SPI_CS, GPIO_OUT);

  for (n = 10; n; n--)
    rcvr_mmc(buf, 1); /* Apply 80 dummy clocks and the card gets ready to receive command */

  ty = 0;
  if (send_cmd(CMD0, 0) == 1)
  { /* Enter Idle state */
    if (send_cmd(CMD8, 0x1AA) == 1)
    {                   /* SDv2? */
      rcvr_mmc(buf, 4); /* Get trailing return value of R7 resp */
      if (buf[2] == 0x01 && buf[3] == 0xAA)
      { /* The card can work at vdd range of 2.7-3.6V */
        for (tmr = 1000; tmr; tmr--)
        { /* Wait for leaving idle state (ACMD41 with HCS bit) */
          if (send_cmd(ACMD41, 1UL << 30) == 0) break;
          sleep_us(1000);
        }
        if (tmr && send_cmd(CMD58, 0) == 0)
        { /* Check CCS bit in the OCR */
          rcvr_mmc(buf, 4);
          ty = (buf[0] & 0x40) ? CT_SDC2 | CT_BLOCK : CT_SDC2; /* SDv2+ */
        }
      }
    }
    else
    { /* SDv1 or MMCv3 */
      if (send_cmd(ACMD41, 0) <= 1)
      {
        ty = CT_SDC2;
        cmd = ACMD41; /* SDv1 */
      }
      else
      {
        ty = CT_MMC3;
        cmd = CMD1; /* MMCv3 */
      }
      for (tmr = 1000; tmr; tmr--)
      { /* Wait for leaving idle state */
        if (send_cmd(cmd, 0) == 0) break;
        sleep_us(1000);
      }
      if (!tmr || send_cmd(CMD16, 512) != 0) /* Set R/W block length to 512 */
        ty = 0;
    }
  }
  CardType = ty;
  Status = CardType != 0;

  deselect();

  if (Status)
  {
    // switch to fast spi clock
    spi_init(SD_SPICH, SD_SPI_BAUDRATE);
  }

  return Status;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

bool MMC_disk_read(uint8_t *buff,     /* Pointer to the data buffer to store read data */
                   int sector,        /* Start sector number (LBA) */
                   unsigned int count /* Sector count (1..128) */
)
{
  uint8_t cmd;
  uint32_t sect = (uint32_t)sector;

  if (!MMC_disk_ready()) return false;

  if (!(CardType & CT_BLOCK)) sect *= 512; /* Convert LBA to byte address if needed */

  cmd = count > 1 ? CMD18 : CMD17; /*  READ_MULTIPLE_BLOCK : READ_SINGLE_BLOCK */
  if (send_cmd(cmd, sect) == 0)
  {
    do
    {
      if (!rcvr_datablock(buff, 512)) break;
      buff += 512;
    } while (--count);
    if (cmd == CMD18) send_cmd(CMD12, 0); /* STOP_TRANSMISSION */
  }
  deselect();

  return (count == 0);
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

bool MMC_disk_write(const uint8_t *buff, /* Pointer to the data to be written */
                    int sector,          /* Start sector number (LBA) */
                    unsigned int count   /* Sector count (1..128) */
)
{
  uint32_t sect = (uint32_t)sector;

  if (!MMC_disk_ready()) return false;

  if (!(CardType & CT_BLOCK)) sect *= 512; /* Convert LBA to byte address if needed */

  if (count == 1)
  {                                  /* Single block write */
    if ((send_cmd(CMD24, sect) == 0) /* WRITE_BLOCK */
        && xmit_datablock(buff, 0xFE))
      count = 0;
  }
  else
  { /* Multiple block write */
    if (CardType & CT_SDC) send_cmd(ACMD23, count);
    if (send_cmd(CMD25, sect) == 0)
    { /* WRITE_MULTIPLE_BLOCK */
      do
      {
        if (!xmit_datablock(buff, 0xFC)) break;
        buff += 512;
      } while (--count);
      if (!xmit_datablock(0, 0xFD)) /* STOP_TRAN token */
        count = 1;
    }
  }
  deselect();

  return (count == 0);
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

bool MMC_sync(void)
{
  bool res;

  if (!MMC_disk_ready()) return false;

  res = select();

  deselect();

  return res;
}

uint32_t MMC_get_block_size(void) { return 128; }

int32_t MMC_get_sector_count(void)
{
  uint8_t n, csd[16];
  uint32_t cs;
  int32_t count = -1;

  if (!MMC_disk_ready()) return -1;

  if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16))
  {
    if ((csd[0] >> 6) == 1)
    { /* SDC ver 2.00 */
      cs = csd[9] + ((uint16_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
      count = cs << 10;
    }
    else
    { /* SDC ver 1.XX or MMC */
      n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
      cs = (csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint16_t)(csd[6] & 3) << 10) + 1;
      count = cs << (n - 9);
    }
  }

  deselect();

  return count;
}
