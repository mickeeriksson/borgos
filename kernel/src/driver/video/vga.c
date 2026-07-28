#include "driver/video/vga.h"
#include "log.h"
#include "mm.h"

struct vga_video_regs xvga_mode13_320x200x256 =
{
    .seq = {0x03, 0x01, 0x0F, 0x00, 0x0E},
    .crtc = {0x5F,0x4F,0x50,0x82,0x24,0x80,0xBF,0x1F,
             0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x31,
             0x9C,0x8E,0x8F,0x28,0x40,0x96,0x89,0xA3,
             0xFF},
    .gfx =  {0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,
             0xFF},
    .attr = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
             0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
             0x41,0x00,0x0F,0x00,0x00},
    .misc = {0x63}
};

struct vga_video_regs xvga_mode3_80x25_text =
{
    .seq = {0x03, 0x00, 0x03, 0x00, 0x02},
    .crtc = {0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
             0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
             0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
             0xFF},
    .gfx = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
            0xFF},
    .attr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
             0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
             0x0C, 0x00, 0x0F, 0x08, 0x00},
    .misc = {0x67}
};


struct vga_modeinfo vga_mode13_320x200x256 =
{
    //.mode=VGA_MODE_IDX_320x200x256,
    .width=320,
    .height=200,
    .bpp=8,
    .pitch=320,
    .fb_size=(320*200),
    .is_text=0,
    .fontheight=0,
    .vgaregs = {
        .seq = {0x03, 0x01, 0x0F, 0x00, 0x0E},
        .crtc = {0x5F,0x4F,0x50,0x82,0x24,0x80,0xBF,0x1F,
             0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x31,
             0x9C,0x8E,0x8F,0x28,0x40,0x96,0x89,0xA3,
             0xFF},
        .gfx =  {0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,
             0xFF},
        .attr = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                 0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
             0x41,0x00,0x0F,0x00,0x00},
        .misc = {0x63}
    }
};

struct vga_modeinfo vga_mode1_40x25 =
{
    //.mode=
    .width=40,
    .height=25,
    .bpp=0,
    .pitch=80,
    .fb_size=(80*25),
    .is_text=1,
    .fontheight=16,
    .vgaregs = {
        .seq = {0x03, 0x08, 0x03, 0x00, 0x02},
        .crtc = {0x2D, 0x27, 0x28, 0x90, 0x2B, 0xA0, 0xBF, 0x1F,
                 0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0xA0,
                 0x9C, 0x8E, 0x8F, 0x14, 0x1F, 0x96, 0xB9, 0xA3,
                 0xFF},
        .gfx = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
                0xFF},
        .attr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
                 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
                 0x0C, 0x00, 0x0F, 0x08, 0x00},
        .misc = {0x67}
    }
};

struct vga_modeinfo vga_mode3_80x25 =
{
    //.mode=
    .width=80,
    .height=25,
    .bpp=0,
    .pitch=160,
    .fb_size=(160*25),
    .is_text=1,
    .fontheight=16,
    .vgaregs = {
        .seq = {0x03, 0x00, 0x03, 0x00, 0x02},
        .crtc = {0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
             0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
             0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
             0xFF},
        .gfx = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
            0xFF},
        .attr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
             0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
             0x0C, 0x00, 0x0F, 0x08, 0x00},
        .misc = {0x67}
    }
};

struct vga_modeinfo vga_mode3_80x50 =
{
    //.mode=
    .width=80,
    .height=50,
    .bpp=0,
    .pitch=160,
    .fb_size=(160*50),
    .is_text=1,
    .fontheight=8,
    .vgaregs = {
        .seq = {0x03, 0x00, 0x03, 0x00, 0x02},
        .crtc = {0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
             0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
             0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
             0xFF},
        .gfx = {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
            0xFF},
        .attr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
             0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
             0x0C, 0x00, 0x0F, 0x08, 0x00},
        .misc = {0x67}
    }
};


int vga_ports_alive(void) {
    io_outb(0x3D4, 0x0A);           // CRTC index: Cursor Start (harmlöst att peta i)
    uint8_t orig = io_inb(0x3D5);

    io_outb(0x3D5, 0x55);
    uint8_t readback1 = io_inb(0x3D5);
    log_msg("readback1=0x%x\n", readback1);
    io_outb(0x3D5, 0xAA);
    uint8_t readback2 = io_inb(0x3D5);
    log_msg("readback2=0x%x\n", readback2);
    io_outb(0x3D5, orig);            // återställ

    return (readback1 == 0x55 && readback2 == 0xAA);
}


void vga_uppdate_attr(adr_t regbase, int regidx, char val)
{
    // Attribute controller uses a single address port with an internal index/data flip-flop.
    //  Reading STAT1 forces the flip-flop to "expects index next".
    vga_r (regbase, VGA_INSTAT_READ);

    //do actual setting
    vga_w (regbase, VGA_ATT_IW, regidx);
    vga_w (regbase, VGA_ATT_W, val);


    //out 0x3c0 rg | 0x20
    //vga_w (regbase, VGA_ATT_IW, regidx | 0x20);

}

void attr_enable_display(adr_t regbase) {
    // Leaves the flip-flop pointing at "index" and turns the screen back on
    // (bit 5 of the index write = 1 -> Palette Address Source = video memory, i.e. display enabled).
    // Call once after programming all AR registers.

    vga_r (regbase, VGA_INSTAT_READ);
    vga_w (regbase, VGA_ATT_IW, 0x20);
}

void vga_crtc_unlock(adr_t regbase) {
    vga_wcrt(regbase,0x03, vga_rcrt(regbase, 0x03) | 0x80); //unlock CR10 & CR11
    vga_wcrt(regbase,0x11, vga_rcrt(regbase, 0x11) & ~0x80); //unlock CR0-CR7

}

/*
void vga_set_regs_old(adr_t regbase,struct vga_video_regs *vgaregs)
{
    // write MISCELLANEOUS reg
    //IS THIS REALLY NEEDED??? WHAT FOR???
    vga_mm_w (regbase, VGA_MIS_W, vgaregs->misc[0]);

    for(int i=0;i<VGA_NUM_AC_REGS;i++){
        vga_uppdate_attr(regbase, i, vgaregs->attr[i]);
    }
    // unlock CRTC registers
    //vga_wcrt(regbase,0x03, vga_rcrt(regbase, 0x03) | 0x80);
    //vga_wcrt(regbase,0x11, vga_rcrt(regbase, 0x11) & ~0x80);
    vga_crtc_unlock(vgaregbase);

    for(int i=0;i<VGA_NUM_CRTC_REGS;i++){
        vga_wcrt(regbase,i,vgaregs->crtc[i]);
    }

    for(int i=0;i<VGA_NUM_GC_REGS;i++){
        vga_wgfx(regbase,i,vgaregs->gfx[i]);
    }

    for(int i=0;i<VGA_NUM_SEQ_REGS;i++){
        vga_wseq(regbase,i,vgaregs->seq[i]);
    }

    // lock 16-color palette and unblank display
    //IS THIS REALLY NEEDED??? WHAT FOR???
    //vga_io_r(VGA_INSTAT_READ);
    //vga_io_w(VGA_ATT_IW,0x20);
    attr_enable_display() ;
}*/

void vga_set_regs(adr_t regbase,struct vga_video_regs *vgaregs)
{
    // Hold the sequencer in reset while we change things underneath it.
    vga_wseq(regbase,0x00, 0x01);

    //unlock CR0-CR76, CR10 & CR11
    vga_crtc_unlock(regbase);

    //Misc sets clock, etc
    vga_mm_w (regbase, VGA_MIS_W, vgaregs->misc[0]);

    for(int i=0;i<VGA_NUM_SEQ_REGS;i++){
        vga_wseq(regbase,i,vgaregs->seq[i]);
    }
    for(int i=0;i<VGA_NUM_CRTC_REGS;i++){
        vga_wcrt(regbase,i,vgaregs->crtc[i]);
    }
    for(int i=0;i<VGA_NUM_GFX_REGS;i++){
        vga_wgfx(regbase,i,vgaregs->gfx[i]);
    }
    for(int i=0;i<VGA_NUM_ATTR_REGS;i++){
        vga_uppdate_attr(regbase, i, vgaregs->attr[i]);
    }

    attr_enable_display(regbase) ;

    // release sequencer reset
    vga_wseq(regbase,0x00, 0x03);
}


/* Reprograms the CRTC Maximum Scan Line field (CR9 bits 0-4) to
 * height-1, preserving bits 5-7 (Vertical Blank Start bit9 / Line Compare
 * bit9 / Scan Double - none of our modes touch those, but preserving them
 * rather than assuming 0 is cheap insurance). Also re-centers the cursor
 * (CR0A/CR0B) as an underline near the bottom of the new, possibly-shorter
 * cell. Text modes only - callers below already check is_text. */
void vga_set_font_height(adr_t regbase,uint32_t height) {
    uint8_t cr9 = vga_rcrt(regbase,0x09);
    cr9 = (uint8_t)((cr9 & 0xE0u) | ((uint8_t)(height - 1) & 0x1Fu));
    vga_wcrt(regbase,0x09, cr9);
    vga_wcrt(regbase,0x0A, (uint8_t)(height - 2)); /* Cursor Start */
    vga_wcrt(regbase,0x0B, (uint8_t)(height - 1)); /* Cursor End   */

    //g_mode_info.char_height = height;
    //g_mode_info.height = 400u / height; /* both modes' CRTC frame is 400 scanlines */
    //g_mode_info.pitch = g_mode_info.width * 2u;
    //g_mode_info.fb_size = g_mode_info.pitch * g_mode_info.height;
}

/*
 * Uploads a 256-glyph font into display memory plane 2, where the VGA
 * character generator reads glyph shapes from for both text modes: hold the
 * sequencer/graphics controller in "linear plane 2" addressing, write each
 * glyph into its 32-byte-aligned slot (only the first `height` of those 32
 * bytes are used), then restore the normal text-mode addressing. Standard
 * VGA technique, chip-agnostic
 */
void vga_load_font(adr_t regbase,const uint8_t *font_flat, uint32_t height) {
    //if (!g_mode_info.is_text) return STDVGA_ERR_BAD_MODE;

    vga_wseq(regbase,0x02, 0x04); /* Map Mask: write only to plane 2           */
    vga_wseq(regbase,0x04, 0x06); /* Chain-4=0, Odd/Even disabled, Ext Memory=1 */
    vga_wgfx(regbase,0x04, 0x02); /* Read Map Select: plane 2                   */
    vga_wgfx(regbase,0x05, 0x00); /* Odd/Even off, Write Mode 0, Read Mode 0    */
    vga_wgfx(regbase,0x06, 0x04); /* Memory Map=01 (A000:0, 64K), still AN mode */

    /* Font memory is always reached through the legacy 64KB window at
     * physical address 0xA0000, independent of either text mode's normal
     * B800:0 character/attribute window. Assumes 0xA0000-0xAFFFF is mapped
     * at that same address in your kernel's address space, which any
     * kernel handling VGA text/graphics memory already needs anyway. */
    volatile uint8_t *plane2 = (volatile uint8_t *)(uintptr_t) P2V(0xA0000);

    for (int c = 0; c < 256; c++) {
        for (uint32_t r = 0; r < height; r++) {
            plane2[c * 32 + (int)r] = font_flat[c * (int)height + (int)r];
        }
    }

    vga_wseq(regbase,0x02, 0x03); /* restore: writes to planes 0 (chars) + 1 (attrs) */
    vga_wseq(regbase,0x04, 0x02); /* restore: Odd/Even addressing enabled            */
    vga_wgfx(regbase,0x04, 0x00); /* restore: Read Map Select = plane 0              */
    vga_wgfx(regbase,0x05, 0x10); /* restore: Odd/Even addressing                    */
    vga_wgfx(regbase,0x06, 0x0E); /* restore: Memory Map=3 (B800:0, CGA-style)       */
    /* Both STDVGA_MODE_TEXT_40x25 and STDVGA_MODE_TEXT_80x25 use this same
     * GC6 value (MODE1_GFX == MODE3_GFX), so one hardcoded restore value
     * is correct for either. */

    //set_char_height(height);
    vga_set_font_height(regbase,height);
    //return OK;
}












/*
void vga_set_mode(adr_t regbase, int mode)
{
    if(mode==0x13){
        vga_set_regs(regbase,&vga_mode13_320x200x256);
    }
}


void vga_set_plane(adr_t regbase,unsigned p)
{
    unsigned char pmask;

    p &= 3;
    pmask = 1 << p;
    // set read plane
    vga_wgfx(regbase,4,p);
    // set write plane
    vga_wseq(regbase,2,pmask);
}
*/

/*****************************************************************************
VGA framebuffer is at A000:0000, B000:0000, or B800:0000
depending on bits in GC 6
*****************************************************************************/
/*
adr_t vga_get_fb_seg(adr_t regbase)
{
    adr_t seg;

    //outportb(VGA_GC_INDEX, 6);
    //seg = inportb(VGA_GC_DATA);
    seg = vga_rgfx(regbase,6);
    seg >>= 2;
    seg &= 3;
    switch(seg)
    {
        case 0:
        case 1:
            seg = 0xA0000;
            break;
        case 2:
            seg = 0xB0000;
            break;
        case 3:
            seg = 0xB8000;
            break;
    }
    return seg;
}
*/

/*****************************************************************************
write font to plane 2, (P4) (assuming planes are named P1, P2, P4, P8)
*****************************************************************************/
/*
void vga_write_font(adr_t regbase,adr_t fb,unsigned char *buf, unsigned font_height)
{
    unsigned char seq2, seq4, gc4, gc5, gc6;
    unsigned i;

    // save registers
    set_plane() modifies GC 4 and SEQ 2, so save them as well
    seq2= vga_rseq(regbase,2);
    seq4 = vga_rseq(regbase,4);
    // turn off even-odd addressing (set flat addressing) assume: chain-4 addressing already off
    vga_wseq(regbase,4,seq4| 0x04);
    gc4= vga_rgfx(regbase,4);

    gc5=vga_rgfx(regbase,5);
    // turn off even-odd addressing
    vga_wgfx(regbase,5,gc5 & ~0x10);

    gc6 = vga_rgfx(regbase,6);
    // turn off even-odd addressing
    vga_wgfx(regbase,6,gc6 & ~0x02);

    // write font to plane P4
    vga_set_plane(regbase,2);


    //adr_t fb_adr = vga_get_fb_seg(regbase);
    adr_t fb_adr = fb;
    //hardcode

    //override to use mmio-bar
    //fb_adr = 0xFC000000;
    //fb_adr = 0x10000000;


    // write font 0
    for(i = 0; i < 256; i++)
    {
        adr_t dst_off = 0x4000 * 0 + i * 32;
        unsigned char *src = buf;
        //vmemwr(0x4000 * 0 + i * 32, buf, font_height);
        //_vmemwr(get_fb_seg(), dst_off, src, count);
        //kprintf("fb_adr = %#x\n",fb_adr);
        uint8_t *dchar = (uint8_t *) (fb_adr+dst_off);
        for(int i=0;i<font_height;i++){
            dchar[i]=src[i];
        }


        buf += font_height;
    }
    //#if 0
    //    // write font 1
    //	for(i = 0; i < 256; i++)
    //	{
    //		vmemwr(16384u * 1 + i * 32, buf, font_height);
    //		buf += font_height;
    //	}
    //#endif
    // restore registers
    vga_wseq(regbase,2,seq2);
    vga_wseq(regbase,4,seq4);
    vga_wgfx(regbase,4,gc4);
    vga_wgfx(regbase,5,gc5);
    vga_wgfx(regbase,6,gc6);

}
*/
