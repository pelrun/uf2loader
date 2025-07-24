#ifndef CONFIG_H
#define CONFIG_H

// GPIOs for SPI interface (SD card)
#define SD_SPI0         0
#define SD_SCLK_PIN     18
#define SD_MOSI_PIN     19
#define SD_MISO_PIN     16
#define SD_CS_PIN       17
#define SD_DET_PIN 22

#define LCD_SPI1    1
#define LCD_SCK_PIN 10
#define LCD_MOSI_PIN 11
#define LCD_MISO_PIN 12
#define LCD_CS_PIN 13
#define LCD_DC_PIN 14
#define LCD_RST_PIN 15

// GPIOs for audio output
#define AUDIO_LEFT     28
#define AUDIO_RIGHT    27

// GPIOs for buttons
#define NEXT_BUTTON    2
#define PART_BUTTON    3

// Pico-internal GPIOs
#define PICO_PS        23
#define LED_PIN        25

#endif // CONFIG_H
