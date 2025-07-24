#ifndef _SD_CARD
#define _SD_CARD

#include <stdint.h>
#include <stdbool.h>

#define _CMD(x) (0x40 | (x))

enum sd_command_e
{
  SD_GO_IDLE_STATE         = _CMD(0),
  SD_ALL_SEND_CID          = _CMD(2),
  SD_SET_RELATIVE_ADDR     = _CMD(3),
  SD_SET_DSR               = _CMD(4),
  SDIO_SEND_OP_COND        = _CMD(5),
  SD_SWITCH_FUNC           = _CMD(6),
  SD_SELECT_DESELECT_CARD  = _CMD(7),
  SD_SEND_IF_COND          = _CMD(8),
  SD_SEND_CSD              = _CMD(9),
  SD_SEND_CID              = _CMD(10),
  SD_VOLTAGE_SWITCH        = _CMD(11),
  SD_STOP_TRANSMISSION     = _CMD(12),
  SD_SEND_STATUS           = _CMD(13),
  SD_GO_INACTIVE_STATE     = _CMD(15),
  SD_SET_BLOCKLEN          = _CMD(16),
  SD_READ_SINGLE_BLOCK     = _CMD(17),
  SD_READ_MULTIPLE_BLOCK   = _CMD(18),
  SD_SEND_TUNING_BLOCK     = _CMD(19),
  SD_SPEED_CLASS_CONTROL   = _CMD(20),
  SD_SET_BLOCK_COUNT       = _CMD(23),
  SD_WRITE_SINGLE_BLOCK    = _CMD(24),
  SD_WRITE_MULTIPLE_BLOCK  = _CMD(25),
  SD_PROGRAM_CSD           = _CMD(27),
  SD_SET_WRITE_PROT        = _CMD(28),
  SD_CLR_WRITE_PROT        = _CMD(29),
  SD_SEND_WRITE_PROT       = _CMD(30),
  SD_ERASE_WR_BLK_START    = _CMD(32),
  SD_ERASE_WR_BLK_END      = _CMD(33),
  SD_ERASE                 = _CMD(38),
  SD_LOCK_UNLOCK           = _CMD(42),
  SD_READ_EXTR_SINGLE      = _CMD(48),
  SD_WRITE_EXTR_SINGLE     = _CMD(49),
  SDIO_RW_DIRECT           = _CMD(52),
  SDIO_RW_EXTENDED         = _CMD(53),
  SD_APP_CMD               = _CMD(55),
  SD_GEN_CMD               = _CMD(56),
  SD_READ_EXTR_MULTI       = _CMD(58),
  SD_WRITE_EXTR_MULTI      = _CMD(59),

  SD_SET_BUS_WIDTH         = _CMD(6),
  SD_STATUS                = _CMD(13),
  SD_SEND_NUM_WR_BLOCKS    = _CMD(22),
  SD_SET_WR_BLK_ERASE_COUN = _CMD(23),
  SD_SEND_OP_COND          = _CMD(41),
  SD_SET_CLR_CARD_DETECT   = _CMD(42),
  SD_SEND_SCR              = _CMD(51),
};
#undef _CMD

#define SD_DATA_TOKEN 0xFE
#define SD_DATA_ACCEPTED 0x05
#define SD_RESPONSE_IDLE 0x01

#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS 17
#define PIN_SCK 18
#define PIN_MOSI 19

#define SD_CARD_FREQ_KHZ 20000

bool sdcard_init(void);
bool sdcard_write_sector(uint32_t sector, const uint8_t *buffer);
bool sdcard_read_sector(uint32_t sector, uint8_t *buffer);

#endif
