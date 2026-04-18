#ifndef _DRIVER_VIDEO_EGA_H_
#define _DRIVER_VIDEO_EGA_H_

#include "types.h"

/* Hardware text mode color constants. */
enum ega_color_16 {
    EGA_COLOR16_BLACK = 0,
    EGA_COLOR16_BLUE = 1,
    EGA_COLOR16_GREEN = 2,
    EGA_COLOR16_CYAN = 3,
    EGA_COLOR16_RED = 4,
    EGA_COLOR16_MAGENTA = 5,
    EGA_COLOR16_BROWN = 6,
    EGA_COLOR16_LIGHT_GREY = 7,
    EGA_COLOR16_DARK_GREY = 8,
    EGA_COLOR16_LIGHT_BLUE = 9,
    EGA_COLOR16_LIGHT_GREEN = 10,
    EGA_COLOR16_LIGHT_CYAN = 11,
    EGA_COLOR16_LIGHT_RED = 12,
    EGA_COLOR16_LIGHT_MAGENTA = 13,
    EGA_COLOR16_LIGHT_BROWN = 14,
    EGA_COLOR16_WHITE = 15,
};

static inline uint8_t ega_entry_color_16(enum ega_color_16 fg, enum ega_color_16 bg){
    return fg | bg << 4;
}

static inline uint16_t ega_entry(unsigned char uc, uint8_t color){
    return (uint16_t) uc | (uint16_t) color << 8;
}

#endif