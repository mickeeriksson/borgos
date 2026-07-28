#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/video/vga.h"
#include "error.h"
#include "cpu/mmio.h"
#include "mm.h"
#include "delay.h"

#define CIRRUS_MMIO_VGA_OFFSET 0x1c000

typedef struct gd5446_device {
    struct fb_info fbinfo;
} gd5446_device_t;

const struct pci_device_id gd5446_pci_idents[] = {
    PCI_DEVICE(0x1013, 0x00B8,"Cirrus Logic GD5446"),   //Cirrus Logic
//        PCI_DEVICE(0x1234, 0x1111),   //Qemu VGA
    PCI_DEVICE_END,
};

/*
void gd5446_crtc_unlock(adr_t vgaregbase) {
    //crtc_write(0x11, crtc_read(0x11) & 0x7Fu);
    //vga_wcrt(vgaregbase,0x11, vga_rcrt(vgaregbase, 0x11) & ~0x80);
    vga_crtc_unlock(vgaregbase);
}


static void gd5446_atc_write(adr_t vgaregbase,uint8_t idx, uint8_t val)
{
    // Attribute Controller – kräver läsning av IS1 för att återställa adress-flip-flop
    //inb(GD_INPUT_STATUS1);   // Reset flip-flop → adress-läge
    vga_r (vgaregbase, 0x3DA);
    //outb(GD_ATC_ADDR, idx);
    vga_w (vgaregbase, VGA_ATT_IW,idx);
    //outb(GD_ATC_ADDR, val);
    vga_w (vgaregbase, VGA_ATT_W,val);
}


void gd_5446_atc_enable(adr_t vgaregbase)
{
    //inb(GD_INPUT_STATUS1);
    vga_r (vgaregbase, 0x3DA);  // Reset flip-flop → adress-läge
    //outb(GD_ATC_ADDR, 0x20); // PAS-bit: aktivera palett
    vga_w (vgaregbase, VGA_ATT_W,0x20);

}
void gd5446_set_vgamode(adr_t vgaregbase, int mode) {
    if (mode==1322) {
        vga_set_regs(vgaregbase,&vga_mode13_320x200x256);
        return;
    }
    if (mode==13) {
        //vga_set_regs(vgaregbase,&vga_mode13_320x200x256);
        //seq_write(0x00, 0x01);  //async reset
        vga_wseq(vgaregbase,0x00,0x01);   //async reset

        vga_set_regs(vgaregbase,&vga_mode13_320x200x256);



        //outb(GD_MISC_W, 0x63);
        //vga_mm_w (vgaregbase, VGA_MIS_W, 0x63);
        vga_wmisc(vgaregbase,0x63);  // 25.175 MHz
        vga_wseq(vgaregbase,0x00,0x03);   //Normal operation
        vga_wseq(vgaregbase,0x01,0x01);   //
        vga_wseq(vgaregbase,0x02,0x0F);   //
        vga_wseq(vgaregbase,0x03,0x00);   //
        vga_wseq(vgaregbase,0x04,0x0E);   //


        //crtc_unlock();
        gd5446_crtc_unlock(vgaregbase);

        static const uint8_t m13_cr[25] = {
            0x5F,0x4F,0x50,0x82,0x54,0x80,0xBF,0x1F,
            0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x00,
            0x9C,0x8E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF
        };
        for (int i = 0; i < 25; i++) {
            //crtc_write(i, m13_cr[i]);
            vga_wcrt(vgaregbase,i,m13_cr[i]);
        }

        static const uint8_t m13_gr[9] = {
            0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF
        };
        for (int i = 0; i < 9; i++) {
            //gfx_write(i, m13_gr[i]);
            vga_wgfx(vgaregbase,i,m13_gr[i]);
        }

        log_msg(".\n");

        for (int i = 0; i < 16; i++) {
            //atc_write(i, i);
            gd5446_atc_write(vgaregbase,i,i);
        }
        //atc_write(0x10, 0x41);
        //atc_write(0x11, 0x00);
        //atc_write(0x12, 0x0F);
        //atc_write(0x13, 0x00);
        //atc_write(0x14, 0x00);
        gd5446_atc_write(vgaregbase, 0x10, 0x41);
        gd5446_atc_write(vgaregbase, 0x11, 0x00);
        gd5446_atc_write(vgaregbase, 0x12, 0x0F);
        gd5446_atc_write(vgaregbase, 0x13, 0x00);
        gd5446_atc_write(vgaregbase, 0x14, 0x00);
        //atc_enable();
        gd_5446_atc_enable(vgaregbase);

        //outb(GD_DAC_MASK, 0xFF);
        vga_w (vgaregbase, 0x3C6,0xFF);
        //seq_write(GD_SR_HICOLOR_DAC, 0x00);
        vga_wseq(vgaregbase,0x0F,0x00);   //


        return;
    }
    PANIC("UNKNOWN MODE");
}
*/

int gd5446_pci_probe(device_t *dev) {
    uint8_t bar = 1;
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    log_msg("gd5446_pci_probe\n");

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "GD5446 PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }

    // Begär minnesregionen från PCI-enheten
    if (pci_request_region(pcidev, &pcidev->bar[bar], "gd5446_driver")) {
        pci_disable_device(pcidev);
        return -EBUSY;
    }


    adr_t mmio_base;
    // Mappa fysiskt PCI-minne till virtuellt kernel-minne (MMIO)
    mmio_base = pci_iomap(pcidev, &pcidev->bar[bar], 0);
    if (!mmio_base) {
        pci_release_region(pcidev, &pcidev->bar[bar]);
        pci_disable_device(pcidev);
        PANIC("UNABLE to map mmio_base via pci_iomap");
        return -ENOMEM;
    }
    log_msg("GOT mmio_base = 0x%lx\n", mmio_base);


    log_msg("BAR 0 (FB)   = 0x%lx  size=0x%lx\n",pcidev->bar[0].addr,pcidev->bar[0].size);
    log_msg("BAR 1 (MMIO) = 0x%lx  size=0x%lx\n",pcidev->bar[1].addr,pcidev->bar[1].size);

    pci_config_regs_t* regs = (pci_config_regs_t*) pcidev->ecam_config_reg_adr;
    uint16_t command = regs->command;
    adr_t vgaregbase = 0;

    adr_t b1=mmio_base;
    vgaregbase=b1-0x3C0;   //0x3c0 will be added in (vga.h) calls to vga_rseq,etc..,


    log_msg("GD5446 Command = 0x%lx\n",command);
    if (command & 0x0001) {
        log_msg("I/O space is enabled!\n");
        if (vga_ports_alive()) {
            log_msg("VGA Ports is alive\n");
            vgaregbase = 0;
        }
    }


    vga_wseq(vgaregbase,0x06, 0x92);
    uint8_t sr06 = vga_rseq(vgaregbase,0x06);   // Lås upp utökade register (SR06 = 0x12)
    log_msg("SR06 = %#x \n",sr06);

    // 5. Läs Chip ID
    uint8_t cr27 = vga_rcrt(vgaregbase,0x27);   // Läs CR27, ChipId
    log_msg("CR27 = %#x \n",cr27);

    // 6. Verifiera (GD5446 ska ha de 4 högsta bitarna satta till 1011, dvs 0xB0-0xBF)
    if ((cr27 & 0xF0) == 0xB0) {
        // MMIO fungerar och chippet är identifierat!
        log_msg("This Cirrus Logic has ChipId = 0x%x, Seems correct!\n",cr27);
    }else {
        log_msg("This Cirrus Logic has a strange ChipId 0x%x, QEMU not implemented?\n",cr27);
        PANIC("UNABLE TO INIT GD5446");
    }

    log_msg("*********************************************************\n");
    log_msg("***** DONT USE gd5446 ON QEMU use std-vga instead *******\n");
    log_msg("***** gd5446 emulation in QEMU is very buggy      *******\n");
    log_msg("*********************************************************\n");


    //TestCode to trigger bad refresh on 320x200 pn cirrus on vmware
    gd5446_device_t* priv = kmalloc(sizeof(gd5446_device_t),0);
    dev->priv = priv;
    struct fb_info *fbinfo = &priv->fbinfo;
    fbinfo->fbops = kmalloc(sizeof(struct fb_ops),0);

/*
    //gd5446_set_vgamode(vgaregbase, 13);
    vga_set_regs(vgaregbase,&vga_mode13_320x200x256);

    volatile uint8_t* fb = (uint8_t*) P2V(0xA0000);  //B8000 for text
    for(int r=5;r<195;r++){
        for(int c=10;c<310;c++){
            //fb[r*80+c]=vga_entry('X', vga_entry_color_16(VGA_COLOR16_LIGHT_GREY, VGA_COLOR16_BLACK));
            fb[r*320+c]=0;
        }
    }

    __asm__ volatile ("wbinvd" ::: "memory"); //clear CPU cache

    for(int r=5;r<195;r++){
        for(int c=10;c<310;c++){
            fb[r*320+c]=2;
        }
        mdelay(2);
        //log_msg("r=%d\n",r);
    }

    __asm__ volatile ("wbinvd" ::: "memory");
    for(int r=180;r<195;r++){
        for(int c=10;c<18;c++){
            log_msg("byte is 0x%x \n",fb[r*320+c]);
        }
    }


    fbinfo->type=VFB_INDEXED;
    fbinfo->fbaddr=P2V(0xA0000);
        fbinfo->fbpitch=320;
        fbinfo->fbwidth=320;
        fbinfo->fbheight=200;
        fbinfo->fbbpp = 8;

        fbinfo->currow=0;
        fbinfo->curcol=0;
        fbinfo->curcolor=VGA_COLOR16_RED;

        //only used for rgb
        fbinfo->fbbytespp = fbinfo->fbpitch / fbinfo->fbwidth;
        fbinfo->fbfgrgbcol=VGA_COLOR16_RED;
        fbinfo->fbbgrgbcol=VGA_COLOR16_BLACK;
        fbinfo->textrows = fbinfo->fbheight/16;
        fbinfo->textcols = fbinfo->fbwidth/8;


        fbinfo->fbops->cls = &fb_rgb_cls;
        fbinfo->fbops->putchar = &fb_rgb_putchar;
        fbinfo->fbops->scroll = &fb_rgb_scroll;
        //fbinfo->curcolor = vga_entry_color_16(VGA_COLOR16_LIGHT_BLUE, VGA_COLOR16_BLACK);
    fbinfo->fbops->cls(fbinfo);
    kconsole_init_vfb(fbinfo);

    kprintf("HEJ i 320x200\n");
    kprintf("Ny Rad\n");
*/

    //log_msg("DONE");

    return 0;
}

int gd5446_pci_attach(device_t *pdev) {
    log_msg("gd5446_pci_attach\n");
    return 0;
}

struct pcidriver gd5446_pci_driver = {
    .driver.name = "Cirrus Logic GD 5446 Graphics Adapter",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = gd5446_pci_probe,
    .driver.attach = gd5446_pci_attach,
    .idents = (void *) &gd5446_pci_idents,
};

REGISTER_DRIVER(gd5446driver, "gd5446",&gd5446_pci_driver);

