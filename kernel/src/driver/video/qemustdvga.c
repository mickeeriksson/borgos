#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/video/vga.h"
#include "driver/video/bochsvbe.h"
#include "error.h"
#include "cpu/mmio.h"
#include "mm.h"
#include "delay.h"

//#define CIRRUS_MMIO_VGA_OFFSET 0x1c000

#define EMULATIONMODE_VGA       1
#define EMULATIONMODE_BOCHSVBE  2


typedef struct gqemustdvga_device {
    struct fb_info fbinfo;
    adr_t vgaregbase;
    int current_emulationmode;
    uint16_t bochsvbe_version;
    adr_t vbe_regbase;
    adr_t vbe_fbadr;
    adr_t edidbase;

} qemustdvga_device_t;

const struct pci_device_id qemustdvga_pci_idents[] = {
    PCI_DEVICE(0x1234, 0x1111,"QEMU STD VGA"),   //Cirrus Logic
//        PCI_DEVICE(0x1234, 0x1111),   //Qemu VGA
    PCI_DEVICE_END,
};


RESULT qemustdvga_set_vgamode(device_t *dev,vga_modeinfo_t *vgamode) {
    qemustdvga_device_t* priv = dev->priv;
    struct fb_info *fbinfo=&priv->fbinfo;

    struct vga_video_regs *vgaregs = &vgamode->vgaregs;
    // make sure CRCT settings remain unlocked, on next use
    vgaregs->crtc[0x03] |= 0x80;
    vgaregs->crtc[0x11] &= ~0x80;


    vga_set_regs(priv->vgaregbase,vgaregs);



    if (vgamode->is_text==0) {
        fbinfo->type=VFB_INDEXED;
        fbinfo->fbaddr=P2V(0xA0000);
    }else {
        fbinfo->type=VFB_TEXT;
        fbinfo->fbaddr=P2V(0xB8000);
    }
    fbinfo->fbwidth=vgamode->width;
    fbinfo->fbheight=vgamode->height;
    fbinfo->fbpitch=vgamode->pitch;
    fbinfo->fbbpp = vgamode->bpp;
    fbinfo->fontheight = vgamode->fontheight;

    //only used for rgb
    if (vgamode->is_text==0) {
        fbinfo->fbbytespp=fbinfo->fbpitch / fbinfo->fbwidth;
        fbinfo->fbfgrgbcol=VGA_COLOR16_RED;
        fbinfo->fbbgrgbcol=VGA_COLOR16_BLACK;
        fbinfo->textrows = fbinfo->fbheight/16;
        fbinfo->textcols = fbinfo->fbwidth/8;

        fbinfo->fbops->cls = &fb_rgb_cls;
        fbinfo->fbops->putchar = &fb_rgb_putchar;
        fbinfo->fbops->scroll = &fb_rgb_scroll;

    }else {
        fbinfo->fbbytespp = 0;
        fbinfo->fbfgrgbcol=0;
        fbinfo->fbbgrgbcol=0;
        fbinfo->textrows = 0;
        fbinfo->textcols = 0;

        fbinfo->fbops->cls = &fb_text_cls;
        fbinfo->fbops->putchar = &fb_text_putchar;
        fbinfo->fbops->scroll = &fb_text_scroll;
    }

    fbinfo->currow=0;
    fbinfo->curcol=0;
    //fbinfo->curcolor = vga_entry_color_16(VGA_COLOR16_LIGHT_BLUE, VGA_COLOR16_BLACK);
    fbinfo->curcolor=VGA_COLOR16_RED;

    fbinfo->fbops->cls(fbinfo);
    kconsole_init_vfb(fbinfo);

    return OK;
}



RESULT qemustdvga_set_mode(device_t *dev,int mode) {
    qemustdvga_device_t* priv = dev->priv;
    if (!dev->initialized)
        return ERROR;

    if (priv->current_emulationmode==EMULATIONMODE_BOCHSVBE) {
        if (priv->bochsvbe_version>0) { //vbe is detected
            bochsvbe_disable(priv->vbe_regbase);
        }
    }


    switch (mode) {
        case VGA_MODE_IDX_320x200x256:
            return qemustdvga_set_vgamode(dev,&vga_mode13_320x200x256);
        case VGA_MODE_TEXT_80x25:
            qemustdvga_set_vgamode(dev,&vga_mode3_80x25);
            //load_font(priv->vgaregbase,&stdvga_default_font_8x16[0][0],16);
            vga_load_font(priv->vgaregbase,&vga_g_8x16_font[0],16);
            return OK;
        case VGA_MODE_TEXT_80x50:
            qemustdvga_set_vgamode(dev,&vga_mode3_80x50);
            //load_font(priv->vgaregbase,&stdvga_default_font_8x8[0][0],8);
            vga_load_font(priv->vgaregbase,&vga_g_8x8_font[0],8);
            return OK;
        case VGA_MODE_TEXT_40x25:
            qemustdvga_set_vgamode(dev,&vga_mode1_40x25);
            //load_font(priv->vgaregbase,&stdvga_default_font_8x16[0][0],16);
            vga_load_font(priv->vgaregbase,&vga_g_8x16_font[0],16);
            return OK;
        default:
            return ERROR;
    }
    priv->current_emulationmode=EMULATIONMODE_VGA;
    return OK;
}

RESULT qemustdvga_set_vbemode(device_t *dev,uint16_t width,uint16_t height,uint16_t bpp) {
    qemustdvga_device_t* priv = dev->priv;
    if (!dev->initialized)
        return ERROR;

    if (priv->bochsvbe_version==0) {
        log_msg("BOCHS VBE NOT SUPPORTED\n");
        PANIC("BOCHS VBE NOT SUPPORTED\n");
    }
    bochsvbe_set_mode(priv->vbe_regbase,width,height,bpp);

    struct fb_info *fbinfo=&priv->fbinfo;

    fbinfo->type=VFB_RGB;
    //fbinfo->fbaddr=P2V(0xB8000);
    fbinfo->fbaddr = priv->vbe_fbadr;
    fbinfo->fbwidth=width;
    fbinfo->fbheight=height;
    fbinfo->fbpitch=width * (bpp/8);
    fbinfo->fbbpp = bpp;
    fbinfo->fontheight = 16;

    fbinfo->fbbytespp=fbinfo->fbpitch / fbinfo->fbwidth;
    fbinfo->fbfgrgbcol=0x00FFFDD0;
    fbinfo->fbbgrgbcol=0x00333333;
    fbinfo->textrows = fbinfo->fbheight/16;
    fbinfo->textcols = fbinfo->fbwidth/8;

    fbinfo->fbops->cls = &fb_rgb_cls;
    fbinfo->fbops->putchar = &fb_rgb_putchar;
    fbinfo->fbops->scroll = &fb_rgb_scroll;


    fbinfo->currow=0;
    fbinfo->curcol=0;
    //fbinfo->curcolor = vga_entry_color_16(VGA_COLOR16_LIGHT_BLUE, VGA_COLOR16_BLACK);
    fbinfo->curcolor=0x00FFFDD0;

    fbinfo->fbops->cls(fbinfo);
    kconsole_init_vfb(fbinfo);

    priv->current_emulationmode=EMULATIONMODE_BOCHSVBE;
    return OK;
}

/*
void qemustdvga_testmode13(device_t *dev) {
    qemustdvga_device_t* priv = dev->priv;
    log_msg("TEST MODE 0x13\n");
    // set mode 13
    if (! (qemustdvga_set_mode(dev,VGA_MODE_IDX_320x200x256)==OK)) {
        PANIC("Error setting mode");
    }
    if (priv->fbinfo.fbaddr==0) {
        PANIC("fbaddr==0");
    }

    //uint8_t* fb = (uint8_t*) P2V(0xA0000);  //B8000 for text
    uint8_t* fb = (uint8_t*) priv->fbinfo.fbaddr;
    for(int r=5;r<195;r++){
        for(int c=10;c<310;c++){
            fb[r*320+c]=0;
        }
    }
    for(int r=5;r<195;r++){
        for(int c=10;c<310;c++){
            fb[r*320+c]=2;
        }
    }
    kprintf("HEJ i 320x200\n");
    kprintf("Ny Rad\n");
    log_msg("MODE 13 Done!\n");
}

void qemustdvga_testmode3(device_t *dev) {
    qemustdvga_device_t* priv = dev->priv;
    log_msg("TEST MODE 0x03\n");

    // set mode 3
    if (! (qemustdvga_set_mode(dev,VGA_MODE_TEXT_80x25)==OK)) {
        PANIC("Error setting mode");
    }
    if (priv->fbinfo.fbaddr==0) {
        PANIC("fbaddr==0");
    }

    kprintf("HEJ i 80x25\n");
    kprintf("Ny Rad\n");
    log_msg("TEST MODE 3 Done!\n");
}

void qemustdvga_testmode1(device_t *dev) {
    qemustdvga_device_t* priv = dev->priv;
    log_msg("TEST MODE 0x01\n");

    // set mode 1
    if (! (qemustdvga_set_mode(dev,VGA_MODE_TEXT_40x25)==OK)) {
        PANIC("Error setting mode");
    }
    if (priv->fbinfo.fbaddr==0) {
        PANIC("fbaddr==0");
    }

    kprintf("HEJ i 40x25!\n");
    kprintf("1234567890123456789012345678901234567890\n");

    for (int i=0;i<20;i++) {
        kprintf("*row %d\n",i);
    }

    //kprintf("HEJ i 40x25\n");
    //kprintf("Ny Rad\n");
    log_msg("TEST MODE 1 Done!\n");
}

void qemustdvga_testmode80x50(device_t *dev) {
    qemustdvga_device_t* priv = dev->priv;
    log_msg("TEST MODE 0x01\n");

    // set mode 1
    if (! (qemustdvga_set_mode(dev,VGA_MODE_TEXT_80x50)==OK)) {
        PANIC("Error setting mode");
    }
    if (priv->fbinfo.fbaddr==0) {
        PANIC("fbaddr==0");
    }

    kprintf("HEJ i 80x50!\n");
    kprintf("1234567890123456789012345678901234567890\n");

    for (int i=0;i<20;i++) {
        kprintf("*row %d\n",i);
    }

    //kprintf("HEJ i 40x25\n");
    //kprintf("Ny Rad\n");
    log_msg("TEST MODE 80x50 Done!\n");
}

void qemustdvga_testmode1024x768(device_t *dev) {
    qemustdvga_device_t* priv = dev->priv;
    log_msg("TEST MODE 1024x768\n");

    // set mode 1
    if (! (qemustdvga_set_vbemode(dev,1024,768,32)==OK)) {
        PANIC("Error setting mode");
    }
    if (priv->fbinfo.fbaddr==0) {
        PANIC("fbaddr==0");
    }

    kprintf("HEJ i 1024x768!\n");
    kprintf("1234567890123456789012345678901234567890\n");

    for (int i=2;i<39;i++) {
        kprintf("*row %d\n",i);
    }

    //kprintf("HEJ i 40x25\n");
    //kprintf("Ny Rad\n");
    log_msg("TEST MODE 1024x768 Done!\n");
}*/

RESULT qemustdvga_read_edid(device_t *dev, uint8_t* out) {
    qemustdvga_device_t* priv = dev->priv;

    const volatile uint8_t *mmio = (const volatile uint8_t *) priv->edidbase;

    for (int i = 0; i < 128; i++) out[i] = mmio[i];

    // Fixed EDID 1.x header, always these exact 8 bytes.
    static const uint8_t header[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    for (int i = 0; i < 8; i++) {
        if (out[i] != header[i]) {
            return ERROR;
        }
    }

    // Whole-block checksum: bytes 0-127 must sum to 0 mod 256.
    uint8_t sum = 0;
    for (int i = 0; i < 128; i++) {
        sum = (uint8_t)(sum + out[i]);
    }
    if (sum != 0) {
        return ERROR;
    }
    return OK;
}

int qemustdvga_pci_probe(device_t *dev) {
    uint8_t bar = 2;
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    log_msg("qemustdvga_pci_probe\n");

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "qemustdvga PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }

    // Begär minnesregionen från PCI-enheten
    if (pci_request_region(pcidev, &pcidev->bar[bar], "qemustdvga_driver")) {
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


    adr_t fb_base;
    // Mappa fysiskt PCI-minne till virtuellt kernel-minne (MMIO)
    fb_base = pci_iomap(pcidev, &pcidev->bar[0], 0);
    if (!mmio_base) {
        pci_release_region(pcidev, &pcidev->bar[bar]);
        pci_disable_device(pcidev);
        PANIC("UNABLE to map mmio_base via pci_iomap");
        return -ENOMEM;
    }
    log_msg("GOT fb_base = 0x%lx\n", fb_base);


    //PCILOG(pcidev, "GD5446 MMIO mappad till 0x%lx\n", mmio_base);

    //uint8_t* vgammio = (uint8_t*) mmio_base;

    log_msg("BAR 0 (FB)   = 0x%lx  size=0x%lx\n",pcidev->bar[0].addr,pcidev->bar[0].size);
    log_msg("BAR 2 (MMIO) = 0x%lx  size=0x%lx\n",pcidev->bar[2].addr,pcidev->bar[2].size);


    //check command reg.
    pci_config_regs_t* regs = (pci_config_regs_t*) pcidev->ecam_config_reg_adr;
    uint16_t command = regs->command;
    log_msg("Command = 0x%lx\n",command);

    //log_msg("START INIT of VGA mmio=0x%lx\n",vgammio);

    /*
    QEMU mappar de klassiska I/O-portarna till specifika offset-adresser i BAR 2:
    0x0000 - 0x03FF: EDID-data (skärminformation).
    0x0400 - 0x041F: VGA I/O-portar (0x3C0 till 0x3DF) remappade 1:1.
    0x0500 - 0x0515: Bochs VBE-register (Dispi) mappade platt.
    BAR2 + 0x600..0x607   QEMU-extensions (t.ex. endian-flagga)
    Detta innebär att I/O-port 0x3C0 ligger på mmio_base + 0x0400, port 0x3C4 ligger på mmio_base + 0x0404 och så vidare.
    */
//    adr_t b2=mmio_base;
//    adr_t regbase=b1-0x03C0;
    adr_t vgaregbase=mmio_base+0x400-0x3C0;
    log_msg("Regbase (0x3C0) = 0x%x \n",vgaregbase+0x3C0);


    //log_msg("BAR 0 (FB)   = 0x%lx  size=0x%lx\n",pcidev->bar[0].addr,pcidev->bar[0].size);
    //log_msg("BAR 1 (MMIO) = 0x%lx  size=0x%lx\n",pcidev->bar[1].addr,pcidev->bar[1].size);



    log_msg("STDVGA Command = 0x%lx\n",command);
    if (command & 0x0001) {
        log_msg("I/O space is enabled!\n");
        if (vga_ports_alive()) {
            log_msg("VGA Ports is alive\n");
            vgaregbase = 0;
        }
    }

    qemustdvga_device_t* priv = kmalloc(sizeof(qemustdvga_device_t),0);
    dev->priv = priv;
    struct fb_info *fbinfo = &priv->fbinfo;
    fbinfo->fbops = kmalloc(sizeof(struct fb_ops),0);
    priv->vgaregbase=vgaregbase;
    priv->current_emulationmode=EMULATIONMODE_BOCHSVBE;  //assume we got a VBE framebuffer from boot.
    priv->bochsvbe_version=0;
    priv->vbe_fbadr=fb_base;
    priv->vbe_regbase=0;
    priv->edidbase=mmio_base+0x0;
    dev->initialized = 1;

    //TEST BOCHS VBE
    priv->vbe_regbase=mmio_base+0x500;
    uint16_t vbe_version = bochsvbe_getversion(priv->vbe_regbase) ;
    log_msg("GOT VBE Version = 0x%lx\n",vbe_version);
    if (vbe_version > 0) {
        priv->bochsvbe_version=vbe_version;
    }else {
        PANIC("VBE version unavailable");
    }

    //uint8_t edid[256];
    //qemustdvga_read_edid(dev, &edid[0]);

/*
    mdelay(1000);
    qemustdvga_testmode13(dev);
    mdelay(1000);
    qemustdvga_testmode1024x768(dev);
    mdelay(1000);
    qemustdvga_testmode3(dev);
    mdelay(1000);
    qemustdvga_testmode1(dev);
    mdelay(1000);
    qemustdvga_testmode80x50(dev);
    mdelay(1000);
  */

    //Set QEMU display to 1280x1024
    if (! (qemustdvga_set_vbemode(dev,1280,1024,32)==OK)) {
        PANIC("Error setting mode");
    }
    if (priv->fbinfo.fbaddr==0) {
        PANIC("fbaddr==0");
    }

    kprintf("Reset to 1280x1024!\n");

    return 0;
}

int qemustdvga_pci_attach(device_t *pdev) {
    log_msg("qemustdvga_pci_attach\n");
    return 0;
}

struct pcidriver qemustdvga_pci_driver = {
    .driver.name = "QEMU STD VGA",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = qemustdvga_pci_probe,
    .driver.attach = qemustdvga_pci_attach,
    .idents = (void *) &qemustdvga_pci_idents,
};

REGISTER_DRIVER(qemustdvgadriver, "qemustdvga",&qemustdvga_pci_driver);

//void gd5446_driver_register(void) {
//    extern void hwdb_driverdb_add(driver_t* driver);
//}
