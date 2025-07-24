#include <stdint.h>

#include "pico/time.h"

#include "hardware/gpio.h"
#include "hardware/spi.h"

#include "sdcard.h"

#define SD_CARD_FREQ_INIT 300

static inline void sdcard_cs(bool value) { gpio_put(PIN_CS, value); }

static void sdcard_spi_init(void)
{
  spi_init(SPI_PORT, SD_CARD_FREQ_INIT * 1000);

  gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

  gpio_set_function(PIN_CS, GPIO_FUNC_SIO);
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_set_outover(PIN_CS, GPIO_OVERRIDE_INVERT);

  sdcard_cs(false);
}

static inline int sdcard_write(const void *buffer, size_t len)
{
  return spi_write_blocking(SPI_PORT, buffer, len);
}

static inline uint8_t sdcard_read(void *buffer, size_t len)
{
  return spi_read_blocking(SPI_PORT, 0xFF, buffer, len);
}

static inline int sdcard_send_command(uint8_t cmd, uint32_t data, uint8_t crc)
{
  uint32_t rdata = __builtin_bswap32(data);

  sdcard_write(&cmd, 1);
  sdcard_write(&rdata, 4);
  sdcard_write(&crc, 1);

  uint8_t result = 0xFF;

  for (uint16_t i = 1000; (i > 0) && (result & 0x80); i++)
  {
    sdcard_read(&result, 1);
  }

  return result;
}

static inline void sdcard_cmd_end(void) { sdcard_write((uint8_t[]){0xFF}, 1); }

static void sdcard_wait_for_idle(void)
{
  uint8_t result = 0;

  for (int i = 1000; (i > 0) && (result != 0xFF); i++)
  {
    sdcard_read(&result, 1);
  }
}

static bool sdcard_cmd0(void)
{
  bool result = sdcard_send_command(SD_GO_IDLE_STATE, 0, 0x95) == SD_RESPONSE_IDLE;

  sdcard_cmd_end();

  return result;
}

static bool sdcard_cmd8(void)
{
  bool result = sdcard_send_command(SD_SEND_IF_COND, 0x1AA, 0x87) == SD_RESPONSE_IDLE;

  uint8_t r7[4];
  sdcard_read(&r7, 4);

  sdcard_cmd_end();

  return result && (r7[3] == 0xAA) && ((r7[2] & 0xF) == 0x01);
}

static void sdcard_cmd55(void)
{
  sdcard_send_command(SD_APP_CMD, 0, 0x65);
  sdcard_cmd_end();
}

static bool sdcard_acmd41(void)
{
  int result = SD_RESPONSE_IDLE;

  while (result == SD_RESPONSE_IDLE)
  {
    sdcard_cmd55();
    result = sdcard_send_command(SD_SEND_OP_COND, 0x40000000, 0x77);
    sdcard_cmd_end();
  }

  return (result == 0);
}

static bool sdcard_cmd58(void)
{
  // hmmmm
  return true;

  // bool result = sdcard_send_command(SD_READ_EXTR_MULTI, 0, 0xFD) != 0;

  // uint8_t r3[4];
  // sdcard_read(&r3, 4);

  // sdcard_cmd_end();

  // return !(result | !(r3[0] & 0x80));
}

static bool sdcard_cmd16()
{
  bool result = sdcard_send_command(SD_SET_BLOCKLEN, 0x200, 0x15) == 0;
  sdcard_cmd_end();
  return result;
}

static bool sdcard_cmd24(uint32_t sector)
{
  bool result = sdcard_send_command(SD_WRITE_SINGLE_BLOCK, sector, 0xFF) == 0;
  sdcard_cmd_end();
  return result;
}

static bool sdcard_cmd17(uint32_t sector)
{
  bool result = sdcard_send_command(SD_READ_SINGLE_BLOCK, sector, 0xFF) == 0;
  sdcard_cmd_end();
  return result;
}

bool sdcard_init(void)
{
  sdcard_spi_init();

  for (uint8_t i = 0; i < 15; ++i)
  {
    // Send >74 SCLK pulses with CS high
    sdcard_cmd_end();
  }

  sdcard_cs(true);

  if (sdcard_cmd0() && sdcard_cmd8() && sdcard_acmd41() && sdcard_cmd58() && sdcard_cmd16())
  {
    spi_init(SPI_PORT, SD_CARD_FREQ_KHZ * 1000);
    return true;
  }

  sdcard_cs(false);
  return false;
}

bool sdcard_write_sector(uint32_t sector, const uint8_t *buffer)
{
  sdcard_cs(true);

  if (!sdcard_cmd24(sector))
  {
    sdcard_cs(false);
    return false;
  }

  sdcard_write((uint8_t[]){SD_DATA_TOKEN}, 1);
  sdcard_write(buffer, 512);
  sdcard_write((uint16_t[]){0xFFFF}, 2);

  uint8_t r1;
  sdcard_read(&r1, 1);
  if ((r1 & 0x1F) != SD_DATA_ACCEPTED)
  {
    sdcard_cs(false);
    return false;
  }

  sdcard_wait_for_idle();
  sdcard_cs(false);

  return true;
}

bool sdcard_read_sector(uint32_t sector, uint8_t *buffer)
{
  uint16_t crc;

  sdcard_cs(true);

  if (!sdcard_cmd17(sector))
  {
    sdcard_cs(false);
    return false;
  }

  uint8_t r1 = 0;
  for (int i = 1000; (i > 0) && (r1 != SD_DATA_TOKEN); i++)
  {
    sdcard_read(&r1, 1);
  }

  if (r1 != SD_DATA_TOKEN)
  {
    sdcard_cs(false);
    return false;
  }

  sdcard_read(buffer, 512);
  sdcard_read(&crc, 2);

  sdcard_cs(false);

  return true;
}
