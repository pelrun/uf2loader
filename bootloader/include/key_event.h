#ifndef KEY_EVENT_H
#define KEY_EVENT_H

#include "i2ckbd.h"
#include <stdio.h>
#include <string.h>
#include <pico/stdio.h>

typedef enum {
    KEY_ARROW_UP = 0xB5,
    KEY_ARROW_LEFT = 0xB4,
    KEY_ARROW_RIGHT = 0xB7,
    KEY_ARROW_DOWN = 0xB6,
    KEY_BACKSPACE = 0x08,
    KEY_ENTER = 0x0A,
} lv_key_t;

void keypad_init(void);
int keypad_get_key(void);
int keypad_get_battery(void);
#endif // KEY_EVENT_H
