#ifndef I2C_KEYBOARD_H
#define I2C_KEYBOARD_H
#include <pico/stdlib.h>
#include <pico/platform.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>

#define I2C_KBD_MOD i2c1
#define I2C_KBD_SDA 6
#define I2C_KBD_SCL 7

#define I2C_KBD_SPEED  10000 // if dual i2c, then the speed of keyboard i2c should be 10khz

#define I2C_KBD_ADDR 0x1F

void init_i2c_kbd();
int read_i2c_kbd();
int read_battery();

#endif