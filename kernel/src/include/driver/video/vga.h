#ifndef _DRIVER_VIDEO_VGA_H_
#define _DRIVER_VIDEO_VGA_H_

#include "types.h"
#include "cpu/mmio.h"
#include "cpu/io.h"

/* VGA index register ports */
#define VGA_ATT_IW  	0x3C0	/* Attribute Controller Index & Data Write Register */
#define VGA_CRT_IC  	0x3D4	/* CRT Controller Index - color emulation */
#define VGA_SEQ_I   	0x3C4	/* Sequencer Index */
#define VGA_GFX_I   	0x3CE	/* Graphics Controller Index */

/* VGA data register ports */
#define VGA_CRT_DC  	0x3D5	/* CRT Controller Data Register - color emulation */
#define VGA_ATT_W   	0x3C0	/* Attribute Controller Data Write Register */
#define VGA_ATT_R   	0x3C1	/* Attribute Controller Data Read Register */
#define VGA_MIS_W   	0x3C2	/* Misc Output Write Register */
#define VGA_SEQ_D   	0x3C5	/* Sequencer Data Register */
#define VGA_GFX_D   	0x3CF	/* Graphics Controller Data Register */

#define VGA_MISC_R      0x3CC   /* misc write */
#define VGA_MISC_W      0x3C2   /* Misc read */


#define	VGA_INSTAT_READ		0x3DA /* Flip-flop register */

#define	VGA_NUM_SEQ_REGS	5
#define	VGA_NUM_CRTC_REGS	25
#define	VGA_NUM_GFX_REGS		9
#define	VGA_NUM_ATTR_REGS		21

#define VGA_MODE_TEXT_40x25      0x01
#define VGA_MODE_TEXT_80x25      0x03
#define VGA_MODE_IDX_320x200x256 0x13
#define VGA_MODE_TEXT_80x50      0xFF03



struct vga_video_regs
{
    uint8_t seq[VGA_NUM_SEQ_REGS];      // Sequencer Regs
    uint8_t crtc[VGA_NUM_CRTC_REGS];     //CRTC Regs
    uint8_t gfx[VGA_NUM_GFX_REGS];       //Graphics Controller Regs
    uint8_t attr[VGA_NUM_ATTR_REGS];       //Attribute Controller regs
    uint8_t misc[1];                    //Misc regs
};

typedef struct vga_modeinfo{
    //vgamode_t mode;
    uint32_t width;       /* visible pixels/columns                        */
    uint32_t height;      /* visible pixel rows (or text rows for text mode)*/
    uint32_t bpp;          /* bits per pixel; 0 for text mode               */
    uint32_t pitch;        /* bytes between the start of consecutive rows   */
    uint32_t fb_size;      /* bytes needed for one full frame               */
    int      is_text;
    uint8_t  fontheight;
    struct vga_video_regs  vgaregs;
} vga_modeinfo_t;





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

static inline unsigned char vga_io_r (unsigned short port)
{
    //return readb (regbase + port);
    //kprintf("VGA read from %#x \n",(regbase + port));
    uint8_t val = io_inb(port);
    return val;
}

static inline void vga_io_w (unsigned short port, unsigned char data)
{
    //writeb (val, regbase + port);
    //mmio_write8(regbase + port,val);
    io_outb(port, data);
}


static inline unsigned char vga_mm_r (adr_t regbase, unsigned short port)
{
    //Regbase will point to an area of size 0x1ff
    //area is equalent of port 0x3C0 to 0x3DF, therefore do some calculation
    //adr_t mmioadr = regbase + port - 0x3C0;
    adr_t mmioadr = regbase + port;
    return mmio_read8(mmioadr);
}

static inline void vga_mm_w (adr_t regbase, unsigned short port, unsigned char val)
{
    //Regbase will point to an area of size 0x1ff
    //area is equalent of port 0x3C0 to 0x3DF, therefore do some calculation
    //adr_t mmioadr = regbase + port - 0x3C0;
    adr_t mmioadr = regbase + port;
    //kprintf("VGA write to %#x (port=%#x) = val\n",mmioadr,port,val);
    mmio_write8(mmioadr,val);
}

static inline unsigned char vga_r (adr_t regbase, unsigned short port)
{
    if (regbase){
        return vga_mm_r (regbase, port);
    }else{
        //PANIC("NOT IMPLEMENTED!");
        return vga_io_r (port);
        //return 0;
    }
}

static inline void vga_w (adr_t regbase, unsigned short port, unsigned char val)
{
    if (regbase){
        vga_mm_w (regbase, port, val);
    }else{
        //PANIC("NOT IMPLEMENTED!");
        vga_io_w (port, val);
    }
}


static inline unsigned char vga_rmisc (adr_t regbase)
{
    //vga_w (regbase, VGA_CRT_IC, reg);
    return vga_r (regbase, VGA_MISC_R);
    //    return 0;
}
static inline void vga_wmisc (adr_t regbase, unsigned char val)
{
    vga_w (regbase, VGA_MISC_W, val);
}



static inline unsigned char vga_rcrt (adr_t regbase, unsigned char reg)
{
    vga_w (regbase, VGA_CRT_IC, reg);
    return vga_r (regbase, VGA_CRT_DC);
//    return 0;
}
static inline void vga_wcrt (adr_t regbase, unsigned char reg, unsigned char val)
{
//#ifdef VGA_OUTW_WRITE
//vga_w_fast (regbase, VGA_CRT_IC, reg, val);
//#else
    vga_w (regbase, VGA_CRT_IC, reg);
    vga_w (regbase, VGA_CRT_DC, val);
//#endif /* VGA_OUTW_WRITE */
}

static inline unsigned char vga_rseq (adr_t regbase, unsigned char reg)
{
    vga_w (regbase, VGA_SEQ_I, reg);
    return vga_r (regbase, VGA_SEQ_D);
//    return 0;
}
static inline void vga_wseq (adr_t regbase, unsigned char reg, unsigned char val)
{
//#ifdef VGA_OUTW_WRITE
//vga_w_fast (regbase, VGA_SEQ_I, reg, val);
//#else
    vga_w (regbase, VGA_SEQ_I, reg);
    vga_w (regbase, VGA_SEQ_D, val);
//#endif /* VGA_OUTW_WRITE */
}

static inline unsigned char vga_rgfx (adr_t regbase, unsigned char reg)
{
    vga_w (regbase, VGA_GFX_I, reg);
    return vga_r (regbase, VGA_GFX_D);
}

static inline void vga_wgfx (adr_t regbase, unsigned char reg, unsigned char val)
{
//#ifdef VGA_OUTW_WRITE
//vga_w_fast (regbase, VGA_GFX_I, reg, val);
//#else
    vga_w (regbase, VGA_GFX_I, reg);
    vga_w (regbase, VGA_GFX_D, val);
//#endif /* VGA_OUTW_WRITE */
}

static inline unsigned char vga_rattr (adr_t regbase, unsigned char reg)
{
    vga_w (regbase, VGA_ATT_IW, reg);
    return vga_r (regbase, VGA_ATT_R);
}

static inline void vga_wattr (adr_t regbase, unsigned char reg, unsigned char val)
{
    vga_w (regbase, VGA_ATT_IW, reg);
    vga_w (regbase, VGA_ATT_W, val);
}


//extern struct vga_video_regs xvga_mode13_320x200x256 ;
//extern struct vga_video_regs xvga_mode1_40x25_text;
//extern struct vga_video_regs xvga_mode3_80x25_text ;
//extern struct vga_video_regs xvga_mode_80x50_text;
extern struct vga_modeinfo vga_mode13_320x200x256;
extern struct vga_modeinfo vga_mode3_80x25;
extern struct vga_modeinfo vga_mode3_80x50;
extern struct vga_modeinfo vga_mode1_40x25;

extern unsigned char vga_g_8x8_font[2048];
extern unsigned char vga_g_8x16_font[4096];

extern unsigned char stdvga_default_font_8x8[256][8];
extern unsigned char stdvga_default_font_8x16[256][16];

//extern void vga_uppdate_attr(adr_t regbase, int regidx, char val);
extern void vga_set_regs(adr_t regbase,struct vga_video_regs *vgaregs);
//extern void vga_set_mode(adr_t regbase, int mode);
//extern void vga_set_plane(adr_t regbase,unsigned p);
////extern void vga_write_font(adr_t regbase,unsigned char *buf, unsigned font_height);
//void vga_write_font(adr_t regbase,adr_t fb,unsigned char *buf, unsigned font_height);

extern void vga_set_font_height(adr_t regbase,uint32_t height);
extern void vga_load_font(adr_t regbase,const uint8_t *fontdata, uint32_t height);

extern int vga_ports_alive(void);

#endif