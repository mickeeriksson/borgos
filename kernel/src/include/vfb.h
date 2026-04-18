#ifndef _FB_H_
#define _FB_H_

#include "types.h"

enum enumfbtype {
    VFB_TEXT = 1,
    VFB_RGB = 2
};

struct fb_info;

struct fb_ops {
    int (*cls)(struct fb_info *fbinfo);
    int (*putchar)(struct fb_info *fbinfo,int c);
    int (*scroll)(struct fb_info *fbinfo);
};

struct fb_info {
    //TODO LOCK

    enum enumfbtype type;
    adr_t fbaddr;
    uint32_t fbpitch;
    uint32_t fbwidth;
    uint32_t fbheight;
    uint8_t fbbpp;

    uint32_t currow;
    uint32_t curcol;
    uint32_t curcolor;

    //only used for rgb
    uint8_t fbbytespp;
    uint32_t fbfgrgbcol;
    uint32_t fbbgrgbcol;
    uint32_t textrows;
    uint32_t textcols;



    struct fb_ops *fbops;
};


extern int fb_text_init_default_ega(struct fb_info *fbinfo) ;
extern int fb_text_cls(struct fb_info *fbinfo);
extern int fb_text_putcharat(struct fb_info *fbinfo,int c,uint8_t color, uint32_t row, uint32_t col);
extern int fb_text_putchar(struct fb_info *fbinfo,int c);
extern int fb_text_scroll(struct fb_info *fbinfo);

extern int fb_rgb_init_default_vga(struct fb_info *fbinfo) ;
extern int fb_rgb_cls(struct fb_info *fbinfo);
extern int fb_rgb_scroll(struct fb_info *fbinfo);
extern int fb_rgb_putcharat(struct fb_info *fbinfo,int c,uint32_t rgb, uint32_t row, uint32_t col);
extern int fb_rgb_putchar(struct fb_info *fbinfo,int c);

#endif