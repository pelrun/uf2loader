#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#include <hardware/spi.h>
#include "hardware/timer.h"
#include <ctype.h>
#include <stdio.h>

#include "lcdspi.h"
#include "i2ckbd.h"
#include "pico/multicore.h"
////////////////////**************************************fonts

#include "fonts/font1.h"
unsigned char *MainFont = (unsigned char *) font1;

static int gui_fcolour;
static int gui_bcolour;
static short current_x = 0, current_y = 0; // the current default position for the next char to be written
static short gui_font_width, gui_font_height;
static short hres = 0;
static short vres = 0;
static char s_height;
static char s_width;
int lcd_char_pos = 0;
unsigned char lcd_buffer[320 * 3] = {0};// 1440 = 480*3, 320*3 = 960

void __not_in_flash_func(spi_write_fast)(spi_inst_t *spi, const uint8_t *src, size_t len) {
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
    for (size_t i = 0; i < len; ++i) {
        while (!spi_is_writable(spi))
            tight_loop_contents();
        spi_get_hw(spi)->dr = (uint32_t) src[i];
    }
}

void __not_in_flash_func(spi_finish)(spi_inst_t *spi) {
    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while (spi_is_readable(spi))
        (void) spi_get_hw(spi)->dr;
    while (spi_get_hw(spi)->sr & SPI_SSPSR_BSY_BITS)
        tight_loop_contents();
    while (spi_is_readable(spi))
        (void) spi_get_hw(spi)->dr;

    // Don't leave overrun flag set
    spi_get_hw(spi)->icr = SPI_SSPICR_RORIC_BITS;
}

void set_font() {

    gui_font_width = MainFont[0];
    gui_font_height = MainFont[1];

    s_height = vres / gui_font_height;
    s_width = hres / gui_font_width;
}

void define_region_spi(int xstart, int ystart, int xend, int yend, int rw) {
    unsigned char coord[4];
    lcd_spi_lower_cs();
    gpio_put(Pico_LCD_DC, 0);//gpio_put(Pico_LCD_DC,0);
    hw_send_spi(&(uint8_t) {ILI9341_COLADDRSET}, 1);
    gpio_put(Pico_LCD_DC, 1);
    coord[0] = xstart >> 8;
    coord[1] = xstart;
    coord[2] = xend >> 8;
    coord[3] = xend;
    hw_send_spi(coord, 4);//		HAL_SPI_Transmit(&hspi3,coord,4,500);
    gpio_put(Pico_LCD_DC, 0);
    hw_send_spi(&(uint8_t) {ILI9341_PAGEADDRSET}, 1);
    gpio_put(Pico_LCD_DC, 1);
    coord[0] = ystart >> 8;
    coord[1] = ystart;
    coord[2] = yend >> 8;
    coord[3] = yend;
    hw_send_spi(coord, 4);//		HAL_SPI_Transmit(&hspi3,coord,4,500);
    gpio_put(Pico_LCD_DC, 0);
    if (rw) {
        hw_send_spi(&(uint8_t) {ILI9341_MEMORYWRITE}, 1);
    } else {
        hw_send_spi(&(uint8_t) {ILI9341_RAMRD}, 1);
    }
    gpio_put(Pico_LCD_DC, 1);
}

void read_buffer_spi(int x1, int y1, int x2, int y2, unsigned char *p) {
    int r, N, t;
    unsigned char h, l;
//	PInt(x1);PIntComma(y1);PIntComma(x2);PIntComma(y2);PRet();
    // make sure the coordinates are kept within the display area
    if (x2 <= x1) {
        t = x1;
        x1 = x2;
        x2 = t;
    }
    if (y2 <= y1) {
        t = y1;
        y1 = y2;
        y2 = t;
    }
    if (x1 < 0) x1 = 0;
    if (x1 >= hres) x1 = hres - 1;
    if (x2 < 0) x2 = 0;
    if (x2 >= hres) x2 = hres - 1;
    if (y1 < 0) y1 = 0;
    if (y1 >= vres) y1 = vres - 1;
    if (y2 < 0) y2 = 0;
    if (y2 >= vres) y2 = vres - 1;
    N = (x2 - x1 + 1) * (y2 - y1 + 1) * 3;

    define_region_spi(x1, y1, x2, y2, 0);

    //spi_init(Pico_LCD_SPI_MOD, 6000000);
    spi_set_baudrate(Pico_LCD_SPI_MOD, 6000000);
    //spi_read_data_len(p, 1);
    hw_read_spi((uint8_t *) p, 1);
    r = 0;
    hw_read_spi((uint8_t *) p, N);
    gpio_put(Pico_LCD_DC, 0);
    lcd_spi_raise_cs();
    spi_set_baudrate(Pico_LCD_SPI_MOD, LCD_SPI_SPEED);
    r = 0;

    while (N) {
        h = (uint8_t) p[r + 2];
        l = (uint8_t) p[r];
        p[r] = h;//(h & 0xF8);
        p[r + 2] = l;//(l & 0xF8);
        r += 3;
        N -= 3;
    }
}

void draw_buffer_spi(int x1, int y1, int x2, int y2, unsigned char *p) {
    union colourmap {
        char rgbbytes[4];
        unsigned int rgb;
    } c;
    unsigned char q[3];
    int i, t;
    if (x2 <= x1) {
        t = x1;
        x1 = x2;
        x2 = t;
    }
    if (y2 <= y1) {
        t = y1;
        y1 = y2;
        y2 = t;
    }
    if (x1 < 0) x1 = 0;
    if (x1 >= hres) x1 = hres - 1;
    if (x2 < 0) x2 = 0;
    if (x2 >= hres) x2 = hres - 1;
    if (y1 < 0) y1 = 0;
    if (y1 >= vres) y1 = vres - 1;
    if (y2 < 0) y2 = 0;
    if (y2 >= vres) y2 = vres - 1;
    i = (x2 - x1 + 1) * (y2 - y1 + 1);
    define_region_spi(x1, y1, x2, y2, 1);
    while (i--) {
        c.rgbbytes[0] = *p++; //this order swaps the bytes to match the .BMP file
        c.rgbbytes[1] = *p++;
        c.rgbbytes[2] = *p++;
        // convert the colours to 565 format
        // convert the colours to 565 format
#ifdef ILI9488
        q[0] = c.rgbbytes[2];
        q[1] = c.rgbbytes[1];
        q[2] = c.rgbbytes[0];
#endif
        hw_send_spi(q, 3);
    }
    lcd_spi_raise_cs();

}

//Print the bitmap of a char on the video output
//    x, y - the top left of the char
//    width, height - size of the char's bitmap
//    scale - how much to scale the bitmap
//	  fc, bc - foreground and background colour
//    bitmap - pointer to the bitmap
void draw_bitmap_spi(int x1, int y1, int width, int height, int scale, int fc, int bc, unsigned char *bitmap) {
    int i, j, k, m, n;
    char f[3], b[3];
    int vertCoord, horizCoord, XStart, XEnd, YEnd;
    char *p = 0;
    union colourmap {
        char rgbbytes[4];
        unsigned int rgb;
    } c;

    if (x1 >= hres || y1 >= vres || x1 + width * scale < 0 || y1 + height * scale < 0)return;
    // adjust when part of the bitmap is outside the displayable coordinates
    vertCoord = y1;
    if (y1 < 0) y1 = 0;                                 // the y coord is above the top of the screen
    XStart = x1;
    if (XStart < 0) XStart = 0;                            // the x coord is to the left of the left marginn
    XEnd = x1 + (width * scale) - 1;
    if (XEnd >= hres) XEnd = hres - 1; // the width of the bitmap will extend beyond the right margin
    YEnd = y1 + (height * scale) - 1;
    if (YEnd >= vres) YEnd = vres - 1;// the height of the bitmap will extend beyond the bottom margin

#ifdef ILI9488
    // convert the colours to 565 format
    f[0] = (fc >> 16);
    f[1] = (fc >> 8) & 0xFF;
    f[2] = (fc & 0xFF);
    b[0] = (bc >> 16);
    b[1] = (bc >> 8) & 0xFF;
    b[2] = (bc & 0xFF);

#endif
    //printf("draw_bitmap_spi-> XStart %d, y1 %d, XEnd %d, YEnd %d\n",XStart,y1,XEnd,YEnd);
    define_region_spi(XStart, y1, XEnd, YEnd, 1);

    n = 0;
    for (i = 0; i < height; i++) {                                   // step thru the font scan line by line
        for (j = 0; j < scale; j++) {                                // repeat lines to scale the font
            if (vertCoord++ < 0) continue;                           // we are above the top of the screen
            if (vertCoord > vres) {                                  // we have extended beyond the bottom of the screen
                lcd_spi_raise_cs();                                  //set CS high
                return;
            }
            horizCoord = x1;
            for (k = 0; k < width; k++) {                            // step through each bit in a scan line
                for (m = 0; m < scale; m++) {                        // repeat pixels to scale in the x axis
                    if (horizCoord++ < 0) continue;                  // we have not reached the left margin
                    if (horizCoord > hres) continue;                 // we are beyond the right margin
                    if ((bitmap[((i * width) + k) / 8] >> (((height * width) - ((i * width) + k) - 1) % 8)) & 1) {
                        hw_send_spi((uint8_t *) &f, 3);
                    } else {
                        if (bc == -1) {
                            c.rgbbytes[0] = p[n];
                            c.rgbbytes[1] = p[n + 1];
                            c.rgbbytes[2] = p[n + 2];
#ifdef ILI9488
                            b[0] = c.rgbbytes[2];
                            b[1] = c.rgbbytes[1];
                            b[2] = c.rgbbytes[0];
#endif
                        }
                        hw_send_spi((uint8_t *) &b, 3);
                    }
                    n += 3;
                }
            }
        }
    }
    lcd_spi_raise_cs();                                  //set CS high

}

// Draw a filled rectangle
// this is the basic drawing promitive used by most drawing routines
//    x1, y1, x2, y2 - the coordinates
//    c - the colour
void draw_rect_spi(int x1, int y1, int x2, int y2, int c) {
    // convert the colours to 565 format
    unsigned char col[3];
    if (x1 == x2 && y1 == y2) {
        if (x1 < 0) return;
        if (x1 >= hres) return;
        if (y1 < 0) return;
        if (y1 >= vres) return;
        define_region_spi(x1, y1, x2, y2, 1);
#ifdef ILI9488
        col[0] = (c >> 16);
        col[1] = (c >> 8) & 0xFF;
        col[2] = (c & 0xFF);
#endif
        hw_send_spi(col, 3);
    } else {
        int i, t, y;
        unsigned char *p;
        // make sure the coordinates are kept within the display area
        if (x2 <= x1) {
            t = x1;
            x1 = x2;
            x2 = t;
        }
        if (y2 <= y1) {
            t = y1;
            y1 = y2;
            y2 = t;
        }
        if (x1 < 0) x1 = 0;
        if (x1 >= hres) x1 = hres - 1;
        if (x2 < 0) x2 = 0;
        if (x2 >= hres) x2 = hres - 1;
        if (y1 < 0) y1 = 0;
        if (y1 >= vres) y1 = vres - 1;
        if (y2 < 0) y2 = 0;
        if (y2 >= vres) y2 = vres - 1;
        define_region_spi(x1, y1, x2, y2, 1);
#ifdef ILI9488
        i = x2 - x1 + 1;
        i *= 3;
        p = lcd_buffer;
        col[0] = (c >> 16);
        col[1] = (c >> 8) & 0xFF;
        col[2] = (c & 0xFF);
        for (t = 0; t < i; t += 3) {
            p[t] = col[0];
            p[t + 1] = col[1];
            p[t + 2] = col[2];
        }
        for (y = y1; y <= y2; y++) {
            spi_write_fast(Pico_LCD_SPI_MOD, p, i);
        }
#endif
    }
    spi_finish(Pico_LCD_SPI_MOD);
    lcd_spi_raise_cs();
}

/******************************************************************************************
 Print a char on the LCD display
 Any characters not in the font will print as a space.
 The char is printed at the current location defined by current_x and current_y
*****************************************************************************************/
void lcd_print_char( int fc, int bc, char c, int orientation) {
    unsigned char *p, *fp, *np = NULL;
    int modx, mody, scale = 0x01;
    int height, width;

    // to get the +, - and = chars for font 6 we fudge them by scaling up font 1
    fp = (unsigned char *) MainFont;

    height = fp[1];
    width = fp[0];
    modx = mody = 0;
    //printf("fp %d, c %d ,height %d width %d\n",fp,c, height,width);

    if (c >= fp[2] && c < fp[2] + fp[3]) {
        p = fp + 4 + (int) (((c - fp[2]) * height * width) / 8);
        //printf("p = %d\n",p);
        np = p;

        draw_bitmap_spi(current_x + modx, current_y + mody, width, height, scale, fc, bc, np);
    } else {
        draw_rect_spi(current_x + modx, current_y + mody, current_x + modx + (width * scale),
                      current_y + mody + (height * scale), bc);
    }

    if (orientation == ORIENT_NORMAL) current_x += width * scale;

}

unsigned char scrollbuff[LCD_WIDTH * 3];

void scroll_lcd_spi(int lines) {
    if (lines == 0)return;
    if (lines >= 0) {
        for (int i = 0; i < vres - lines; i++) {
            read_buffer_spi(0, i + lines, hres - 1, i + lines, scrollbuff);
            draw_buffer_spi(0, i, hres - 1, i, scrollbuff);
        }
        draw_rect_spi(0, vres - lines, hres - 1, vres - 1, gui_bcolour); // erase the lines to be scrolled off
    } else {
        lines = -lines;
        for (int i = vres - 1; i >= lines; i--) {
            read_buffer_spi(0, i - lines, hres - 1, i - lines, scrollbuff);
            draw_buffer_spi(0, i, hres - 1, i, scrollbuff);
        }
        draw_rect_spi(0, 0, hres - 1, lines - 1, gui_bcolour); // erase the lines introduced at the top
    }

}

void display_put_c(char c) {
    // if it is printable and it is going to take us off the right hand end of the screen do a CRLF
    if (c >= MainFont[2] && c < MainFont[2] + MainFont[3]) {
        if (current_x + gui_font_width > hres) {
            display_put_c('\r');
            display_put_c('\n');
        }
    }

    // handle the standard control chars
    switch (c) {
        case '\b':
            current_x -= gui_font_width;
            //if (current_x < 0) current_x = 0;
            if (current_x < 0) {  //Go to end of previous line
                current_y -= gui_font_height;                  //Go up one line
                if (current_y < 0) current_y = 0;
                current_x = (s_width - 1) * gui_font_width;  //go to last character
            }
            return;
        case '\r':
            current_x = 0;
            return;
        case '\n':
            current_x = 0;
            current_y += gui_font_height;
            if (current_y + gui_font_height >= vres) {
                scroll_lcd_spi(current_y + gui_font_height - vres);
                current_y -= (current_y + gui_font_height - vres);
            }
            return;
        case '\t':
            do {
                display_put_c(' ');
            } while ((current_x / gui_font_width) % 2);// 2 3 4 8
            return;
    }
    lcd_print_char(gui_fcolour, gui_bcolour, c, ORIENT_NORMAL);// print it
}

/***
 *
****////
char lcd_put_char(char c, int flush) {
    lcd_putc(0, c);
    if (isprint(c)) lcd_char_pos++;
    if (c == '\r') {
        lcd_char_pos = 1;
    }
    return c;
}

void lcd_print_string(char *s) {
    while (*s) {
        if (s[1])lcd_put_char(*s, 0);
        else lcd_put_char(*s, 1);
        s++;
    }
    fflush(stdout);
}

///////=----------------------------------------===//////
void lcd_clear() {
    draw_rect_spi(0, 0, hres - 1, vres - 1, BLACK);
}

void lcd_putc(uint8_t devn, uint8_t c) {
    display_put_c(c);
}

int lcd_getc(uint8_t devn) {
    //i2c keyboard
    int c = read_i2c_kbd();
    return c;
}

unsigned char __not_in_flash_func(hw1_swap_spi)(unsigned char data_out) {
    unsigned char data_in = 0;
    spi_write_read_blocking(spi1, &data_out, &data_in, 1);
    return data_in;
}

void hw_read_spi(unsigned char *buff, int cnt) {
    spi_read_blocking(Pico_LCD_SPI_MOD, 0xff, buff, cnt);
}

void hw_send_spi(const unsigned char *buff, int cnt) {

    spi_write_blocking(Pico_LCD_SPI_MOD, buff, cnt);

}

void pin_set_bit(int pin, unsigned int offset) {
    switch (offset) {
        case LATCLR:
            gpio_set_pulls(pin, false, false);
            gpio_pull_down(pin);
            gpio_put(pin, 0);
            return;
        case LATSET:
            gpio_set_pulls(pin, false, false);
            gpio_pull_up(pin);
            gpio_put(pin, 1);
            return;
        case LATINV:
            gpio_xor_mask(1 << pin);
            return;
        case TRISSET:
            gpio_set_dir(pin, GPIO_IN);
            sleep_us(2);
            return;
        case TRISCLR:
            gpio_set_dir(pin, GPIO_OUT);
            gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_12MA);
            sleep_us(2);
            return;
        case CNPUSET:
            gpio_set_pulls(pin, true, false);
            return;
        case CNPDSET:
            gpio_set_pulls(pin, false, true);
            return;
        case CNPUCLR:
        case CNPDCLR:
            gpio_set_pulls(pin, false, false);
            return;
        case ODCCLR:
            gpio_set_dir(pin, GPIO_OUT);
            gpio_put(pin, 0);
            sleep_us(2);
            return;
        case ODCSET:
            gpio_set_pulls(pin, true, false);
            gpio_set_dir(pin, GPIO_IN);
            sleep_us(2);
            return;
        case ANSELCLR:
            gpio_set_function(pin, GPIO_FUNC_SIO);
            gpio_set_dir(pin, GPIO_IN);
            return;
        default:
            break;
            //printf("Unknown pin_set_bit command");
    }
}

//important for read lcd memory
void reset_controller(void) {
    pin_set_bit(Pico_LCD_RST, LATSET);
    sleep_us(10000);
    pin_set_bit(Pico_LCD_RST, LATCLR);
    sleep_us(10000);
    pin_set_bit(Pico_LCD_RST, LATSET);
    sleep_us(200000);
}


void pico_lcd_init() {
#ifdef ILI9488
    reset_controller();

    hres = 320;
    vres = 320;

    spi_write_command(0xE0); // Positive Gamma Control
    spi_write_data(0x00);
    spi_write_data(0x03);
    spi_write_data(0x09);
    spi_write_data(0x08);
    spi_write_data(0x16);
    spi_write_data(0x0A);
    spi_write_data(0x3F);
    spi_write_data(0x78);
    spi_write_data(0x4C);
    spi_write_data(0x09);
    spi_write_data(0x0A);
    spi_write_data(0x08);
    spi_write_data(0x16);
    spi_write_data(0x1A);
    spi_write_data(0x0F);

    spi_write_command(0XE1); // Negative Gamma Control
    spi_write_data(0x00);
    spi_write_data(0x16);
    spi_write_data(0x19);
    spi_write_data(0x03);
    spi_write_data(0x0F);
    spi_write_data(0x05);
    spi_write_data(0x32);
    spi_write_data(0x45);
    spi_write_data(0x46);
    spi_write_data(0x04);
    spi_write_data(0x0E);
    spi_write_data(0x0D);
    spi_write_data(0x35);
    spi_write_data(0x37);
    spi_write_data(0x0F);

    spi_write_command(0XC0); // Power Control 1
    spi_write_data(0x17);
    spi_write_data(0x15);

    spi_write_command(0xC1); // Power Control 2
    spi_write_data(0x41);

    spi_write_command(0xC5); // VCOM Control
    spi_write_data(0x00);
    spi_write_data(0x12);
    spi_write_data(0x80);

    spi_write_command(TFT_MADCTL); // Memory Access Control
    spi_write_data(0x48); // MX, BGR

    spi_write_command(0x3A); // Pixel Interface Format
    spi_write_data(0x66); // 18 bit colour for SPI

    spi_write_command(0xB0); // Interface Mode Control
    spi_write_data(0x00);

    spi_write_command(0xB1); // Frame Rate Control
    spi_write_data(0xA0);

    spi_write_command(TFT_INVON);

    spi_write_command(0xB4); // Display Inversion Control
    spi_write_data(0x02);

    spi_write_command(0xB6); // Display Function Control
    spi_write_data(0x02);
    spi_write_data(0x02);
    spi_write_data(0x3B);

    spi_write_command(0xB7); // Entry Mode Set
    spi_write_data(0xC6);
    spi_write_command(0xE9);
    spi_write_data(0x00);

    spi_write_command(0xF7); // Adjust Control 3
    spi_write_data(0xA9);
    spi_write_data(0x51);
    spi_write_data(0x2C);
    spi_write_data(0x82);

    spi_write_command(TFT_SLPOUT); //Exit Sleep
    sleep_ms(120);

    spi_write_command(TFT_DISPON); //Display on
    sleep_ms(120);

    spi_write_command(TFT_MADCTL);
    spi_write_cd(ILI9341_MEMCONTROL, 1, ILI9341_Portrait);
#endif
}

void lcd_spi_raise_cs(void) {
    gpio_put(Pico_LCD_CS, 1);
}

void lcd_spi_lower_cs(void) {

    gpio_put(Pico_LCD_CS, 0);

}

void spi_write_data(unsigned char data) {
    gpio_put(Pico_LCD_DC, 1);
    lcd_spi_lower_cs();
    hw_send_spi(&data, 1);
    lcd_spi_raise_cs();
}

void spi_write_data24(uint32_t data) {
    uint8_t data_array[3];
    data_array[0] = data >> 16;
    data_array[1] = (data >> 8) & 0xFF;
    data_array[2] = data & 0xFF;


    gpio_put(Pico_LCD_DC, 1); // Data mode
    gpio_put(Pico_LCD_CS, 0);
    spi_write_blocking(Pico_LCD_SPI_MOD, data_array, 3);
    gpio_put(Pico_LCD_CS, 1);
}

void spi_write_command(unsigned char data) {
    gpio_put(Pico_LCD_DC, 0);
    gpio_put(Pico_LCD_CS, 0);

    spi_write_blocking(Pico_LCD_SPI_MOD, &data, 1);

    gpio_put(Pico_LCD_CS, 1);
}

void spi_write_cd(unsigned char command, int data, ...) {
    int i;
    va_list ap;
    va_start(ap, data);
    spi_write_command(command);
    for (i = 0; i < data; i++) spi_write_data((char) va_arg(ap, int));
    va_end(ap);
}

void lcd_spi_init() {
    // init GPIO
    gpio_init(Pico_LCD_SCK);
    gpio_init(Pico_LCD_TX);
    gpio_init(Pico_LCD_RX);
    gpio_init(Pico_LCD_CS);
    gpio_init(Pico_LCD_DC);
    gpio_init(Pico_LCD_RST);

    gpio_set_dir(Pico_LCD_SCK, GPIO_OUT);
    gpio_set_dir(Pico_LCD_TX, GPIO_OUT);
    //gpio_set_dir(Pico_LCD_RX, GPIO_IN);
    gpio_set_dir(Pico_LCD_CS, GPIO_OUT);
    gpio_set_dir(Pico_LCD_DC, GPIO_OUT);
    gpio_set_dir(Pico_LCD_RST, GPIO_OUT);

    // init SPI
    spi_init(Pico_LCD_SPI_MOD, LCD_SPI_SPEED);
    gpio_set_function(Pico_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(Pico_LCD_TX, GPIO_FUNC_SPI);
    gpio_set_function(Pico_LCD_RX, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled(Pico_LCD_RX, true);

    gpio_put(Pico_LCD_CS, 1);
    gpio_put(Pico_LCD_RST, 1);
}


void lcd_init() {
    lcd_spi_init();
    pico_lcd_init();

    set_font();
    gui_fcolour = GREEN;
    gui_bcolour = BLACK;

}
