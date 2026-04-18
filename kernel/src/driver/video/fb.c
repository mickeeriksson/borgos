#include "types.h"
#include "vfb.h"
#include "error.h"
#include "driver/video/ega.h"
#include "driver/video/vga.h"
#include <string.h>

// TEXT STUFF  ***************************

int fb_text_init_default_ega(struct fb_info *fbinfo) {
    fbinfo->fbops->cls = &fb_text_cls;
    fbinfo->fbops->putchar = &fb_text_putchar;
    fbinfo->fbops->scroll = &fb_text_scroll;
    fbinfo->curcolor = ega_entry_color_16(EGA_COLOR16_LIGHT_GREY, EGA_COLOR16_BLACK);
    fbinfo->fbops->cls(fbinfo);
    kconsole_init_vfb(fbinfo);
    return 0;
}



int fb_text_cls(struct fb_info *fbinfo){
    for(uint32_t r=0;r<fbinfo->fbheight;r++){
        for(uint32_t c=0;c<fbinfo->fbwidth;c++){
            fb_text_putcharat(fbinfo,' ',fbinfo->curcolor, r, c);
        }
    }
    fbinfo->currow = 0;
    fbinfo->curcol = 0;

    return 0;
}

int fb_text_putcharat(struct fb_info *fbinfo,int c,uint8_t color, uint32_t row, uint32_t col){
    uint16_t* fbbuf = (uint16_t*) fbinfo->fbaddr;
    uint32_t pos = row*fbinfo->fbwidth + col;
    //log_msg("fb_text_putcharat %#x (%d), color=%#x entry=%#x\n",c,c,color,ega_entry(c,color));
    fbbuf[pos]=ega_entry(c,color);
    return 0;
}

int fb_text_scroll(struct fb_info *fbinfo){
    uint16_t* fbbuf = (uint16_t*) fbinfo->fbaddr;
    uint32_t linesize = fbinfo->fbwidth;
    uint32_t cpysize = (fbinfo->fbheight-1)*fbinfo->fbwidth;
    for(uint32_t idx=0;idx<cpysize;idx++){
        fbbuf[idx]=fbbuf[idx+linesize];
    }

    //clear last row
    for(uint32_t c=0;c<fbinfo->fbwidth;c++){
        fb_text_putcharat(fbinfo,' ',fbinfo->curcolor, fbinfo->fbheight-1, c);
    }
    fbinfo->currow = fbinfo->fbheight-1;
    fbinfo->curcol = 0;

    return 0;
}

int fb_text_putchar(struct fb_info *fbinfo,int c){
    if(c=='\n'){
        fbinfo->currow++;
        fbinfo->curcol=0;
        if(fbinfo->currow==fbinfo->fbheight){
            fbinfo->fbops->scroll(fbinfo);
        }
        return 0;
    }
    fb_text_putcharat(fbinfo,c, fbinfo->curcolor, fbinfo->currow, fbinfo->curcol);
    fbinfo->curcol++;
    if(fbinfo->curcol>=fbinfo->fbwidth){
        //force newline
        fbinfo->currow++;
        fbinfo->curcol=0;
        if(fbinfo->currow==fbinfo->fbheight){
            fbinfo->fbops->scroll(fbinfo);
        }
        return 0;
    }
    return 0;
}

// RGB STUFF  ***************************

#define CLEARBYTE 0x33

int fb_rgb_init_default_vga(struct fb_info *fbinfo) {
    fbinfo->fbbytespp = fbinfo->fbpitch / fbinfo->fbwidth;
    fbinfo->fbfgrgbcol=0x00FFFDD0;
    fbinfo->fbbgrgbcol=0x00333333;

    fbinfo->textrows = fbinfo->fbheight/16;
    fbinfo->textcols = fbinfo->fbwidth/8;

    fbinfo->fbops->cls = &fb_rgb_cls;
    fbinfo->fbops->putchar = &fb_rgb_putchar;
    fbinfo->fbops->scroll = &fb_rgb_scroll;
    fbinfo->curcolor = vga_entry_color_16(VGA_COLOR16_LIGHT_BLUE, VGA_COLOR16_BLACK);
    fbinfo->fbops->cls(fbinfo);
    kconsole_init_vfb(fbinfo);
    return 0;
}

int fb_rgb_setpixel(struct fb_info *fbinfo, uint16_t row, uint16_t col, uint32_t rgb) {
    uint8_t *pxlptr = (uint8_t*) fbinfo->fbaddr + row * fbinfo->fbpitch + col*fbinfo->fbbytespp;
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = (rgb >> 0)  & 0xFF;
    pxlptr[0] = b;
    pxlptr[1] = g;
    pxlptr[2] = r;
    return 0;
}

int fb_rgb_cls(struct fb_info *fbinfo){
    //uint8_t clearbyte = 0x33;  //to set rgb = 0x00333333
    uint8_t* fbbuf = (uint8_t*) fbinfo->fbaddr;
    size_t copysize = (fbinfo->fbheight)*fbinfo->fbpitch;
    memset(fbbuf, CLEARBYTE,copysize);

    fbinfo->currow = 0;
    fbinfo->curcol = 0;

    return 0;
}

int fb_rgb_scroll(struct fb_info *fbinfo){
    uint8_t fontheight=16;
    uint8_t* fbbuf = (uint8_t*) fbinfo->fbaddr;
    uint8_t* copyfrom = fbbuf+(fbinfo->fbpitch*fontheight);

    size_t copysize = (fbinfo->fbheight-fontheight)*fbinfo->fbpitch;
    memcpy(fbbuf, copyfrom, copysize);

    //clear bootom row
    //uint8_t clearbyte = 0x33;  //to set rgb = 0x00333333
    uint8_t* fbstart = (uint8_t*) fbinfo->fbaddr+ ((fbinfo->textrows-1)*fontheight)*fbinfo->fbpitch;
    uint32_t clearrows = fbinfo->fbheight - ((fbinfo->textrows-1)*fontheight);
    size_t setsize = clearrows*fbinfo->fbpitch;
    memset(fbstart, CLEARBYTE,setsize);


    fbinfo->currow = fbinfo->textrows-1;
    fbinfo->curcol = 0;
    return 0;
}

extern unsigned char vga_g_8x16_font[4096];
int fb_rgb_putcharat(struct fb_info *fbinfo,int c,uint32_t rgb, uint32_t row, uint32_t col){
    uint8_t fontheight=16;

    // 16 fontrows in vga_g_8x16_font
    for (int fr=0;fr<fontheight;fr++) {
        uint8_t pxls = vga_g_8x16_font[c*fontheight+fr];
        for (int bitidx=0;bitidx<8;bitidx++) {
            uint8_t bit = pxls & (1<<(7-bitidx));
            if (bit==0) {
                //set background
                fb_rgb_setpixel(fbinfo, row*fontheight+fr ,col*8+bitidx, fbinfo->fbbgrgbcol);
            }else {
                //set forground
                fb_rgb_setpixel(fbinfo, row*fontheight+fr ,col*8+bitidx, rgb);
            }
        }
    }
    return 0;
}

int fb_rgb_putchar(struct fb_info *fbinfo,int c){
    //uint8_t fontheight=16;
    if(c=='\n'){
        fbinfo->currow++;
        fbinfo->curcol=0;
        if(fbinfo->currow>=fbinfo->textrows){
            fbinfo->fbops->scroll(fbinfo);
        }
        return 0;
    }
    fb_rgb_putcharat(fbinfo,c, fbinfo->fbfgrgbcol, fbinfo->currow, fbinfo->curcol);
    fbinfo->curcol++;
    if(fbinfo->curcol>=fbinfo->textcols){
        //force newline
        fbinfo->currow++;
        fbinfo->curcol=0;
        if(fbinfo->currow>=fbinfo->textrows){
            fbinfo->fbops->scroll(fbinfo);
        }
        return 0;
    }
    return 0;
}

