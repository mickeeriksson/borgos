#ifndef _DRIVER_PCI_PCI_H
#define _DRIVER_PCI_PCI_H

#include "types.h"
#include "log.h"
#include "device.h"
#include "driver/driver.h"

#ifdef __cplusplus
extern "C" {
#endif


#define PCILOG(pcidev,...)    log_msg(__VA_ARGS__)
//#define PCILOG(...)
//#define PCILOG2(...)    log_msg(__VA_ARGS__)
//#define PCILOG2(...)
//#define PCILOG3(...)    log_msg(__VA_ARGS__)
//#define PCILOG3(...)



typedef struct pci_location {
    uint16_t segmentgroup;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
} pci_location_t;

#define MAX_BARS 6

typedef struct pci_config_regs {
    //for header type 0x0
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command ;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist ;
    uint32_t bar[MAX_BARS];
    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_baseadr;
    uint8_t capabilities_ptr;
    uint8_t reserved1[3];
    uint8_t reserved2[4];
    uint8_t interrupt_line ;
    uint8_t interrupt_pin ;
    uint8_t min_grant ;
    uint8_t max_latency ;
} pci_config_regs_t;

//samma register som offsett
#define PCICONFIG_COMMAND           0x04

//utökade configregs för USB. Finns inget om detta i PCI-dokumentationen. KOlla USB-dok
//https://stuff.mit.edu/afs/sipb/contrib/doc/specs/protocol/usb/UHCI11D.PDF page 19
//https://www.intel.com/Assets/PDF/datasheet/290562.pdf page 106
#define PCICONFIG_USB_EXT_SBRN       0x60  // 1byte, Serial Bus Release Number	R/W (oftast RO i praktiken). Anger vilken USB-spec kontrollern följer: 0x10 = USB 1.0, 0x11 = USB 1.1
#define PCICONFIG_USB_EXT_FLADJ      0x61  // 1byte,  Frame Length Adjustment	R/W. Justerar SOF-ramlängden i steg om ca 121 ppm för att kompensera klockavvikelser. Reset-värde brukar vara 0x20
#define PCICONFIG_USB_EXT_USBLEGSUP  0xC0  // 2bytes, USB Legacy Support	R/W. Styr BIOS SMI-trapping för legacy tangentbord/mus (port 60h/64h-emulering). Här tar din drivrutin ownership från BIOS

enum bartype_t {
    BARTYPE_UNKNOWN   = 0,
    BARTYPE_IO        = 1,
    BARTYPE_MEM       = 2,
};
enum barwidth_t {
    BARWIDTH_UNKNOWN  = 0,
    BARWIDTH_32       = 1,
    BARWIDTH_64       = 2,
};
typedef struct pcibar {
    adr_t addr;
    size_t size;
    enum bartype_t bartype;
    enum barwidth_t barwidth;
    uint8_t prefetchable;
} pcibar_t;

typedef struct pcidevice {
    device_t devicebase;
    pci_location_t location;
    adr_t ecam_config_reg_adr;
    pcibar_t bar[MAX_BARS];
} pcidevice_t;

typedef struct pcidriver {
    driver_t driver;
    struct pci_device_id *idents;
} pcidriver_t;

// This structure is used to identify a device. populated from drivers
struct pci_device_id {
    uint16_t vendor;
    uint16_t device;
    char* name;
};
#define PCI_DEVICE(vend, dev, devname) { .vendor = (vend), .device = (dev), .name = (devname) }
#define PCI_DEVICE_END {.vendor = 0xFFFF, .device = 0xFFFF, .name = ""}

extern uint8_t pci_config_read8(pcidevice_t* pcidev,adr_t offsett);
extern void pci_config_write8(pcidevice_t* pcidev,adr_t offsett,uint8_t data);
extern uint16_t pci_config_read16(pcidevice_t* pcidev,adr_t offsett);
extern void pci_config_write16(pcidevice_t* pcidev,adr_t offsett,uint16_t data);
extern uint32_t pci_config_read32(pcidevice_t* pcidev,adr_t offsett);
extern void pci_config_write32(pcidevice_t* pcidev,adr_t offsett,uint32_t data);
extern void pci_config_enumerate_regions(void);
extern void pci_config_driver_init(void) ;
extern void pci_config_parse_bars(pcidevice_t* pcidev,int maxbars);
extern uint32_t pci_config_get_capabillity_ptr(pcidevice_t* pcidev,uint8_t id) ;
extern void pci_config_msi_addirq(pcidevice_t* pcidev,uint32_t capptr,uint8_t msiirq,void* irqhandler,adr_t message_adr_reg32) ;

extern char* pci_class_typename(uint8_t mclass, uint8_t subclass, uint8_t prog_if) ;

extern int pci_request_region(pcidevice_t* pcidev, pcibar_t* pcibar,char* name);
extern int pci_release_region(pcidevice_t* pcidev, pcibar_t* pcibar) ;
extern adr_t pci_iomap(pcidevice_t* pcidev, pcibar_t* pcibar,char* name);
extern int pci_enable_device(pcidevice_t* pcidev);
extern int pci_disable_device(pcidevice_t* pcidev) ;

#ifdef __cplusplus
}
#endif


#endif
