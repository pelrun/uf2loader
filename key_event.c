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
#include <stdio.h>
#include <string.h>
#include <pico/stdio.h>
#include "key_event.h"
#include "debug.h"

void keypad_init(void)
{
    init_i2c_kbd();
}

int keypad_get_key(void)
{   
    int r = read_i2c_kbd();
    if (r < 0) {
        return 0;
    }
    

    int act_key = 0;
    
    /* Translate the keys to LVGL control characters according to your key definitions */
    switch (r) {
        case 0xb5: // Arrow Up
            act_key = KEY_ARROW_UP;
            break;
        case 0xb6: // Arrow Down
            act_key = KEY_ARROW_DOWN;
            break;
        case 0xb4: // Arrow Left
            act_key = KEY_ARROW_LEFT;
            break;
        case 0xb7: // Arrow Right
            act_key = KEY_ARROW_RIGHT;
            break;
        case 0x0A: // Enter
            act_key = KEY_ENTER;
            break;

        // Special Keys
        case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
        case 0x86: case 0x87: case 0x88: case 0x89: case 0x90: // F1-F10 Keys
            DEBUG_PRINT("Warn: F-key unmapped\n");
            act_key = 0;
            break;

        case 0xB1: // ESC
            act_key = 0;
            break;
        case 0x09: // TAB
            act_key = 0;
            break;
        case 0xC1: // Caps Lock
            act_key = 0;
            break;
        case 0xD4: // DEL
            act_key = 0;
            break;
        case 0x08: // Backspace
            act_key = KEY_BACKSPACE;
            break;

        case 0xD0: // brk
            act_key = 0;
            break;
        case 0xD2: // Home
            act_key = 0;
            break;
        case 0xD5: // End
            act_key = 0;
            break;

        case 0x60: case 0x2F: case 0x5C: case 0x2D: case 0x3D:
        case 0x5B: case 0x5D: // `/\-=[] Keys
            act_key = r;
            break;

        case 0x7E: act_key = '~'; break;
        case 0x3F: act_key = '?'; break;
        case 0x7C: act_key = '|'; break;
        case 0x5F: act_key = '_'; break;
        case 0x2B: act_key = '+'; break;
        case 0x7B: act_key = '{'; break;
        case 0x7D: act_key = '}'; break;

        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
        case 0x35: case 0x36: case 0x37: case 0x38: case 0x39: // 0-9 Keys
            act_key = r;
            break;

        case 0x21: case 0x40: case 0x23: case 0x24: case 0x25:
        case 0x5E: case 0x26: case 0x2A: case 0x28: case 0x29: // !@#$%^&*() Keys
            act_key = r;
            break;

        case 0xD1: // Insert
            DEBUG_PRINT("Warn: Insert unmapped\n");
            act_key = 0;
            break;

        case 0x3C: act_key = '<'; break;
        case 0x3E: act_key = '>'; break;

        case 0x3B: case 0x27: case 0x3A: case 0x22: // ;:'"" Keys
            act_key = r;
            break;
        case 0xA5: // CTL
            DEBUG_PRINT("Warn: CTL unmapped\n");
            act_key = 0;
            break;
        case 0x20: // SPACE
            act_key = r;
            break;
        case 0xA1: // ALT
            DEBUG_PRINT("Warn: ALT unmapped\n");
            act_key = 0;
            break;
        case 0xA2: case 0xA3: // RIGHT/LEFT SHIFT
            break;

        default:
            act_key = r;
            break;

    }
    return act_key;
}

