#include "driver/video/bochsvbe.h"
#include "cpu/mmio.h"
#include "cpu/io.h"
#include "error.h"


#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_ID0 0xB0C0  // Bochs VBE Extensions v1
#define VBE_DISPI_ID1 0xB0C1
#define VBE_DISPI_ID2 0xB0C2  // 16-bit DAC stöd, mest relevant baseline
#define VBE_DISPI_ID3 0xB0C3
#define VBE_DISPI_ID4 0xB0C4
#define VBE_DISPI_ID5 0xB0C5  // senaste, stödd av QEMU:s std-vga

#define VBE_DISPI_INDEX_ID          0   // identifiering/version, skriv & läs tillbaka
#define VBE_DISPI_INDEX_XRES        1   // bredd i pixlar
#define VBE_DISPI_INDEX_YRES        2   // höjd i pixlar
#define VBE_DISPI_INDEX_BPP         3   // bitar per pixel
#define VBE_DISPI_INDEX_ENABLE      4   // på/av + LFB-flaggor
#define VBE_DISPI_INDEX_BANK        5   // banknummer (banked mode, oftast irrelevant med LFB)
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_GETCAPS      0x02
#define VBE_DISPI_8BIT_DAC     0x20
#define VBE_DISPI_LFB_ENABLED  0x40  // linjär framebuffer, vill du nästan alltid ha
#define VBE_DISPI_NOCLEARMEM   0x80

static inline void bochsvbe_write(adr_t regbase,uint16_t index, uint16_t value) {
    if (regbase == 0) {
        io_outw(VBE_DISPI_IOPORT_INDEX, index);
        io_outw(VBE_DISPI_IOPORT_DATA, value);
    }else {
        //leftshift index since it is a word (16bytes) instead of 8bytes
        //mmio_write16(regbase+(VBE_DISPI_IOPORT_INDEX<<1),index);
        //mmio_write16(regbase+(VBE_DISPI_IOPORT_DATA<<1),value);
        mmio_write16(regbase+(index<<1),value);
        //PANIC("TODO regbased read");
    }
}

static inline uint16_t bochsvbe_read(adr_t regbase,uint16_t index) {
    if (regbase == 0) {
        io_outw(VBE_DISPI_IOPORT_INDEX, index);
        return io_inw(VBE_DISPI_IOPORT_DATA);
    }else {
        //leftshift index since it is a word (16bytes) instead of 8bytes
        //mmio_write16(regbase+(VBE_DISPI_IOPORT_INDEX<<1),index);
        //return mmio_read16(regbase+(VBE_DISPI_IOPORT_DATA<<1));
        return mmio_read16(regbase+(index<<1));
        //PANIC("TODO regbased read");
        //return 0;
    }
}



/* Gå tillbaka till vanlig VGA-kontroll */
void bochsvbe_disable(adr_t regbase) {
    bochsvbe_write(regbase,VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    /* Nu styr VGA-registren igen — men du MÅSTE
       reprogrammera dem till ett känt läge nedan */
}

/*
 * Returnerar den högsta ID:t som kortet accepterar (t.ex. 0xB0C5),
 * eller 0 om inget BGA-stöd hittades alls.
 *
 * Metod: skriv varje kandidat-ID i fallande ordning, läs tillbaka.
 * Om värdet är oförändrat stöds den versionen (och alla lägre).
 */
uint16_t bochsvbe_getversion(adr_t regbase) {
    static const uint16_t candidates[] = {
        VBE_DISPI_ID5,
        VBE_DISPI_ID4,
        VBE_DISPI_ID3,
        VBE_DISPI_ID2,
        VBE_DISPI_ID1,
        VBE_DISPI_ID0,
    };

    for (int i = 0; i < 6; i++) {
        bochsvbe_write(regbase,VBE_DISPI_INDEX_ID, candidates[i]);
        uint16_t readback = bochsvbe_read(regbase,VBE_DISPI_INDEX_ID);

        if (readback == candidates[i]) {
            return candidates[i]; // hittade högsta stödda version
        }
    }

    return 0; // inget BGA/VBE-stöd — riktigt QEMU std-vga bör alltid nå minst ID2
}

void bochsvbe_set_mode(adr_t regbase,uint16_t width, uint16_t height, uint16_t bpp) {
    bochsvbe_write(regbase,VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    bochsvbe_write(regbase,VBE_DISPI_INDEX_XRES, width);
    bochsvbe_write(regbase,VBE_DISPI_INDEX_YRES, height);
    bochsvbe_write(regbase,VBE_DISPI_INDEX_BPP,  bpp);

    bochsvbe_write(regbase,VBE_DISPI_INDEX_ENABLE,VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}