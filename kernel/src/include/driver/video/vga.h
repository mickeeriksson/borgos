#ifndef _DRIVER_VIDEO_VGA_H_
#define _DRIVER_VIDEO_VGA_H_

#include "types.h"

/* Hardware text mode color constants. */
enum vga_color_16 {
    VGA_COLOR16_BLACK = 0,
    VGA_COLOR16_BLUE = 1,
    VGA_COLOR16_GREEN = 2,
    VGA_COLOR16_CYAN = 3,
    VGA_COLOR16_RED = 4,
    VGA_COLOR16_MAGENTA = 5,
    VGA_COLOR16_BROWN = 6,
    VGA_COLOR16_LIGHT_GREY = 7,
    VGA_COLOR16_DARK_GREY = 8,
    VGA_COLOR16_LIGHT_BLUE = 9,
    VGA_COLOR16_LIGHT_GREEN = 10,
    VGA_COLOR16_LIGHT_CYAN = 11,
    VGA_COLOR16_LIGHT_RED = 12,
    VGA_COLOR16_LIGHT_MAGENTA = 13,
    VGA_COLOR16_LIGHT_BROWN = 14,
    VGA_COLOR16_WHITE = 15,
};

static inline uint8_t vga_entry_color_16(enum vga_color_16 fg, enum vga_color_16 bg){
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color){
    return (uint16_t) uc | (uint16_t) color << 8;
}

#endif