#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/video/vga.h"
#include "error.h"
#include "cpu/mmio.h"
#include "mm.h"

//#define CIRRUS_MMIO_VGA_OFFSET 0x1c000

const struct pci_device_id qemuedu_pci_idents[] = {
    PCI_DEVICE(0x1234, 0x11E8,"QEMU EDU"),   //Cirrus Logic
//        PCI_DEVICE(0x1234, 0x1111),   //Qemu VGA
    PCI_DEVICE_END,
};


#define EDU_ID 0x00
#define EDU_TEST 0x04
#define EDU_IRQ_RAISE 0x60
#define EDU_IRQ_STATUS 0x24
#define EDU_IRQ_CLEAR 0x64

void qemuedu_irqhandler() {
    log_msg("qemuedu_irqhandler\n");
}

int qemuedu_pci_probe(device_t *dev) {
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    log_msg("qemuedu_pci_probe\n");

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "qemuedu PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }

    // Begär minnesregionen från PCI-enheten
    if (pci_request_region(pcidev, &pcidev->bar[0], "qemuedu_driver")) {
        pci_disable_device(pcidev);
        return -EBUSY;
    }

    adr_t reg_base;
    // Mappa fysiskt PCI-minne till virtuellt kernel-minne (MMIO)
    reg_base = pci_iomap(pcidev, &pcidev->bar[0], 0);
    if (!reg_base) {
        pci_release_region(pcidev, &pcidev->bar[0]);
        pci_disable_device(pcidev);
        PANIC("UNABLE to map reg_base via pci_iomap");
        return -ENOMEM;
    }

    pci_config_regs_t* regs = (pci_config_regs_t*) pcidev->ecam_config_reg_adr;
    uint16_t command = regs->command;
    log_msg("Command = 0x%lx\n",command);

    log_msg("GOT reg_base = 0x%lx\n", reg_base);
    log_msg("BAR 0 (MMIO)   = 0x%lx  size=0x%lx\n",pcidev->bar[0].addr,pcidev->bar[0].size);

    uint32_t id = mmio_read32(reg_base + EDU_ID);
    log_msg("GOT id = 0x%x should be 0x10000ED\n", id);

    mmio_write32(reg_base + EDU_TEST, 0xAAAAAAAA);
    uint32_t test = mmio_read32(reg_base + EDU_TEST);
    log_msg("GOT test = 0x%x should be 0x55555555\n", test);

    uint32_t irqline = pci_config_read32(pcidev,0x3C) & 0x000F;
    log_msg("GOT irqline = 0x%x\n", irqline);



     //TESTA IRQ

    uint32_t capptr = pci_config_get_capabillity_ptr(pcidev,0x05);
    log_msg("Found Capability PTR = 0x%x\n", capptr);
    if (capptr>0) {
        uint8_t msiirq=11+100+32; //100 is MSI IRQ OFSETT
        adr_t message_adr_reg32 = 0x0FEE << 20;
        uint32_t core_id = 0x0;
        message_adr_reg32 = message_adr_reg32 | (core_id << 12);
        pci_config_msi_addirq(pcidev,capptr,msiirq,qemuedu_irqhandler,message_adr_reg32) ;
    }

    mmio_write32(reg_base + EDU_IRQ_CLEAR, 0xFFFFFFFF);


    mmio_write32(reg_base + EDU_IRQ_RAISE, 0x42);
    uint32_t status = mmio_read32(reg_base + EDU_IRQ_STATUS);

    // Gör något kul här, t.ex. skriv ut till skärmen
    log_msg("EDU-avbrott mottaget! Status: 0x%x\n", status);
    
    return 0;
}

int qemuedu_pci_attach(device_t *pdev) {
    log_msg("qemuedu_pci_attach\n");
    return 0;
}

struct pcidriver qemuedu_pci_driver = {
    .driver.name = "QEMU STD VGA",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = qemuedu_pci_probe,
    .driver.attach = qemuedu_pci_attach,
    .idents = (void *) &qemuedu_pci_idents,
};

REGISTER_DRIVER(qemuedudriver, "qemuedu",&qemuedu_pci_driver);

//void gd5446_driver_register(void) {
//    extern void hwdb_driverdb_add(driver_t* driver);
//}
