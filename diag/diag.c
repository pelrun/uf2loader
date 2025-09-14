#include <pico.h>
#include <pico/time.h>

#include "i2ckbd.h"

#include "lcdspi.h"
#include "pff.h"

void _Noreturn infinite_loop(void) {
  while (1) {
    tight_loop_contents();
  }
}

#if PICO_RP2040

#define LOADER "BOOT2040.UF2"

#elif PICO_RP2350

#define LOADER "BOOT2350.UF2"

#endif

#define KEY_UP 0xb5
#define KEY_DOWN 0xb6
#define KEY_F1 0x81
#define KEY_F2 0x82
#define KEY_F3 0x83
#define KEY_F4 0x84
#define KEY_F5 0x85
#define KEY_ENTER 0x0A

char *read_bootmode() {
  int key;
  int end_time = time_us_32() + 500000; // 0.5s

#define Q(x) #x "  "
#define KEY(x)                                                                 \
  case KEY_##x:                                                                \
    return Q(x);

  while (((int)time_us_32() - end_time) < 0) {
    key = read_i2c_kbd();
    switch (key) {
      KEY(UP);
      KEY(DOWN);
      KEY(F1);
      KEY(F3);
      KEY(F5);
    }
  }

  return "NONE";
}

#define UI_X (20)

#define UI_Y(line) (20 + 12 * (line))

#define STEP_Y(line) (UI_Y(line + 4))

static inline void fail(int step) {
  lcd_set_cursor(UI_X + 200, STEP_Y(step));
  lcd_print_string_color("FAIL", RED, BLACK);
  infinite_loop();
}

static inline void pass(int step) {
  lcd_set_cursor(UI_X + 200, STEP_Y(step));
  lcd_print_string_color("PASS", GREEN, BLACK);
}

static inline void check_keypress(void) {
  char *keypress = read_bootmode();
  lcd_set_cursor(UI_X + 200, UI_Y(8));
  // draw_rect_spi(UI_X+200, UI_Y(8), 320, UI_Y(9), BLACK);
  lcd_print_string_color(keypress, GREEN, BLACK);
}

int main() {
  char *filename = LOADER;

#ifdef ENABLE_DEBUG
  stdio_init_all();
#endif

  init_i2c_kbd();

  lcd_init();

  lcd_set_cursor(UI_X, UI_Y(0));
  lcd_print_string_color("UF2 Loader Diagnostics " PICO_PROGRAM_VERSION_STRING,
                         WHITE, BLACK);

  lcd_set_cursor(UI_X, STEP_Y(0));
  lcd_print_string_color("SD card init...", WHITE, BLACK);

  lcd_set_cursor(UI_X, STEP_Y(1));
  lcd_print_string_color(LOADER " open...", WHITE, BLACK);

  lcd_set_cursor(UI_X, STEP_Y(2));
  lcd_print_string_color(LOADER " read...", WHITE, BLACK);

  FATFS fs;
  FRESULT fr = FR_NOT_READY;

  // seems to take a couple of attempts from cold start
  for (int retry = 5; retry > 0; retry--) {
    fr = pf_mount(&fs);

    if (fr == FR_OK) {
      break;
    }

    sleep_ms(500);
  }

  if (fr != FR_OK) {
    fail(0);
  }

  pass(0);

  fr = pf_open(filename);

  if (fr != FR_OK) {
    fail(1);
  }

  pass(1);

  char buffer[512];
  unsigned int btr;

  fr = pf_read(buffer, sizeof(buffer), &btr);

  if (fr != FR_OK || btr != sizeof(buffer)) {
    fail(2);
  }

  pass(2);

  lcd_set_cursor(UI_X, UI_Y(8));
  lcd_print_string_color("Key pressed...", WHITE, BLACK);

  while (1) {
    check_keypress();
    sleep_ms(20);
  }
}
