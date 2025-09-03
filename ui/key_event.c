/**
 * PicoCalc SD Firmware Loader
 *
 * Author: Hsuan Han Lai
 * Email: hsuan.han.lai@gmail.com
 * Website: https://hsuanhanlai.com
 * Year: 2025
 *
 * key_event.c
 *
 * Wrapper for post processing dispatch keyboard events
 *
 */

#include "i2ckbd.h"
#include <pico/stdio.h>
#include "key_event.h"
#include "debug.h"

void keypad_init(void)
{
    init_i2c_kbd();
}

int keypad_get_key(void)
{
    return read_i2c_kbd();
}

int keypad_get_battery() {
    int bat_pcnt = read_battery();
    bat_pcnt = bat_pcnt >> 8;
    //int bat_charging = bitRead(bat_pcnt, 7);
    bitClear(bat_pcnt, 7);
    return bat_pcnt;
}