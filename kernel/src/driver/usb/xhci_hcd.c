#include "log.h"
#include "device.h"
#include "driver/driver.h"
#include "driver/pci/pci.h"
#include "driver/usb/usb.h"
#include "error.h"
#include "cpu/mmio.h"
#include "cpu/io.h"
#include "mm.h"
#include "delay.h"


// Compile-time bounds. All driver state is static
// so these cap what the driver will use even if the controller offers more.
#define XHCI_MAX_SLOTS_SUPPORTED    32
#define XHCI_MAX_PROTOCOLS          4
#define XHCI_MAX_ROOT_HUBS          XHCI_MAX_PROTOCOLS
#define XHCI_MAX_PORTS_SUPPORTED    64


// ===== Capability registers ========================================
// xHCI 1.2 table 5-9, section 5.3. Offsets from the MMIO base.
#define XHCI_CAP_CAPLENGTH          0x00   // 8-bit
#define XHCI_CAP_HCIVERSION         0x02   // 16-bit
#define XHCI_CAP_HCSPARAMS1         0x04
#define XHCI_CAP_HCSPARAMS2         0x08
#define XHCI_CAP_HCSPARAMS3         0x0C
#define XHCI_CAP_HCCPARAMS1         0x10
#define XHCI_CAP_DBOFF              0x14
#define XHCI_CAP_RTSOFF             0x18
#define XHCI_CAP_HCCPARAMS2         0x1C

// HCSPARAMS1 - xHCI 1.2 table 5-10.
#define XHCI_HCSP1_MAXSLOTS(x)      ((uint32_t)((x) & 0xFFu))
#define XHCI_HCSP1_MAXINTRS(x)      ((uint32_t)(((x) >> 8) & 0x7FFu))
#define XHCI_HCSP1_MAXPORTS(x)      ((uint32_t)(((x) >> 24) & 0xFFu))

// HCSPARAMS2 - xHCI 1.2 table 5-11. The scratchpad buffer count is split
// across two fields: high order 5 bits at 25:21, low order 5 bits at 31:27.
#define XHCI_HCSP2_ERST_MAX(x)      ((uint32_t)(((x) >> 4) & 0x0Fu))
#define XHCI_HCSP2_SPB_HI(x)        ((uint32_t)(((x) >> 21) & 0x1Fu))
#define XHCI_HCSP2_SPB_LO(x)        ((uint32_t)(((x) >> 27) & 0x1Fu))
#define XHCI_HCSP2_MAX_SCRATCHPAD(x) \
    ((XHCI_HCSP2_SPB_HI(x) << 5) | XHCI_HCSP2_SPB_LO(x))

// HCCPARAMS1 - xHCI 1.2 table 5-13. xECP is a dword offset from the MMIO
// base, not a byte offset (section 5.3.6, worked example).
#define XHCI_HCCP1_AC64             (1u << 0)   // 64-bit addressing capable
#define XHCI_HCCP1_CSZ              (1u << 2)   // 64-byte contexts if set
#define XHCI_HCCP1_PPC              (1u << 3)   // port power control present
//#define XHCI_HCCP1_XECP(x)          ((uint32_t)(((x) >> 16) & 0xFFFFu))

// ===== Operational registers =======================================
// xHCI 1.2 table 5-18. Offsets from the operational base, which is
// mmio_base + CAPLENGTH.
#define XHCI_OP_USBCMD              0x00
#define XHCI_OP_USBSTS              0x04
#define XHCI_OP_PAGESIZE            0x08
#define XHCI_OP_DNCTRL              0x14
#define XHCI_OP_CRCR                0x18   // 64-bit
#define XHCI_OP_DCBAAP              0x30   // 64-bit
#define XHCI_OP_CONFIG              0x38
#define XHCI_OP_PORT_BASE           0x400  // port register sets start here

// USBCMD - xHCI 1.2 table 5-20.
#define XHCI_CMD_RS                 (1u << 0)   // Run/Stop
#define XHCI_CMD_HCRST              (1u << 1)   // Host Controller Reset
#define XHCI_CMD_INTE               (1u << 2)   // Interrupter Enable
#define XHCI_CMD_HSEE               (1u << 3)   // Host System Error Enable

// USBSTS - xHCI 1.2 table 5-21.
#define XHCI_STS_HCH                (1u << 0)   // HCHalted, RO
#define XHCI_STS_HSE                (1u << 2)   // Host System Error, RW1C
#define XHCI_STS_EINT               (1u << 3)   // Event Interrupt, RW1C
#define XHCI_STS_PCD                (1u << 4)   // Port Change Detect, RW1C
#define XHCI_STS_CNR                (1u << 11)  // Controller Not Ready, RO
#define XHCI_STS_HCE                (1u << 12)  // Host Controller Error, RO


#define USB_TRSTRCY  10             // reset recovery time, defined as 10 ms in USB2.0

const struct pci_device_id xhcihcd_pci_idents[] = {
    PCI_DEVICE(0x1B36, 0x000D,"QEMU XHCI Host Controller"),
    PCI_DEVICE(0x1033, 0x0194,"NEC uPD720200 USB 3.0 Host Controller"),
    PCI_DEVICE_END,
};

typedef struct xhci_hcd_device {
    usb_hcd_t  usb_hcd;   //must be first, this way a pointer to usb_hcd is also pointer to usb_hcd...
    adr_t mmio_regbase;
    adr_t io_regbase;
    //uint32_t *stackframe;  //Frame List: 1024 x 32-bit pointers, 4096-byte aligned.
    //adr_t queueframe;       //adress to block of allocated qh's
    //uhci_qh_t* queue[QSLOTS];   // the different queues.
    //uhci_qh_t* curTransfer;   //change this to a linked list...

    uint32_t xECPoff;   // xECP offsett from mmio_regbase

    adr_t reg_op_mmiobase;

} xhci_hcd_device_t;


// ===== Extended capabilities =======================================
// xHCI 1.2 section 7, table 7-2. The next pointer inside each capability is
// a dword offset relative to that capability's own address.
/*
#define XHCI_EXT_CAP_ID(x)          ((uint32_t)((x) & 0xFFu))
#define XHCI_EXT_CAP_NEXT(x)        ((uint32_t)(((x) >> 8) & 0xFFu))

#define XHCI_EXT_CAP_LEGACY         1u
#define XHCI_EXT_CAP_PROTOCOL       2u

// USB Legacy Support - xHCI 1.2 section 7.1, tables 7-4 and 7-5.
#define XHCI_LEGSUP                 0x00u   // USBLEGSUP, at xECP + 0
#define XHCI_LEGCTLSTS              0x04u   // USBLEGCTLSTS, at xECP + 4

#define XHCI_LEGSUP_BIOS_OWNED      (1u << 16)
#define XHCI_LEGSUP_OS_OWNED        (1u << 24)
*/

// ===== Extended capabilities =======================================
// xECP in HCCPARAMS1 and the Next pointer inside each capability are both
// counted in DWORDS, not bytes (xHCI 1.2 section 5.3.6). Getting that wrong
// lands in the middle of some other register and looks like a hardware fault.
#define XHCI_EXT_CAP_ID(x)          ((uint32_t)((x) & 0xFF))
#define XHCI_EXT_CAP_NEXT(x)        ((uint32_t)(((x) >> 8) & 0xFF))
uint32_t xhci_ext_cap_find(xhci_hcd_device_t* xhcidev,uint32_t id) {
    adr_t mmioregbase = xhcidev->mmio_regbase;
    uint32_t offset = xhcidev->xECPoff;
    for (uint32_t guard = 0; offset != 0 && guard < 256u; guard++) {
        uint32_t entry = mmio_read32(mmioregbase+offset);
        log_msg("XHCI EXT CAP entry= %#x\n",entry);
        if (XHCI_EXT_CAP_ID(entry) == id) {
            return offset;
        }
        uint32_t next = XHCI_EXT_CAP_NEXT(entry);
        if (next == 0) {
            return 0;
        }
        offset += next * 4u;
    }
    return 0;
}

// ===== BIOS handoff (AD-7) =========================================
//
// On real hardware firmware often owns the controller so it can offer USB
// keyboards in BIOS setup, and it traps the controller's interrupts as SMIs.
// Resetting the controller out from under System Management Mode hangs the
// machine, so ownership is transferred through the protocol below first.
// QEMU's qemu-xhci exposes no legacy capability at all, in which case this
// is a no-op and NOT an error.

#define XHCI_EXT_CAP_LEGACY_SUPPORT_ID         0x01

USBRESULT xhci_disable_legacy_support(xhci_hcd_device_t* xhcidev) {
    adr_t mmioregbase = xhcidev->mmio_regbase;
    uint32_t capoff = xhci_ext_cap_find(xhcidev,XHCI_EXT_CAP_LEGACY_SUPPORT_ID);  // XHCI_EXT_CAP_LEGACY_SUPPORT_ID has ID =1
    if (!capoff) {
        log_msg("This XHCI device does not support legacy support, skip disable!");
        return USB_OK;
    }

    uint32_t legsup = mmio_read32(mmioregbase + capoff + 0x00);
    log_msg("legsup = %#x\n",legsup);

    PANIC("TODO Implement xhci_disable_legacy_support");
    /*
    int forced = 0;
    uint32_t legsup = xhci_read32(cap + XHCI_LEGSUP);

    if (legsup & XHCI_LEGSUP_BIOS_OWNED) {
        xhci_write32(cap + XHCI_LEGSUP, legsup | XHCI_LEGSUP_OS_OWNED);

        uint32_t spins = XHCI_SPIN_HANDOFF;
        while (spins--) {
            legsup = xhci_read32(cap + XHCI_LEGSUP);
            if (!(legsup & XHCI_LEGSUP_BIOS_OWNED)) {
                break;
            }
            xhci_relax();
        }

        if (legsup & XHCI_LEGSUP_BIOS_OWNED) {
            // Firmware never let go. Take it anyway: refusing here would
            // leave USB unusable on a machine with broken firmware, which is
            // a worse outcome than a forced handoff. This is what Linux does.
            legsup &= ~(uint32_t)XHCI_LEGSUP_BIOS_OWNED;
            legsup |= XHCI_LEGSUP_OS_OWNED;
            xhci_write32(cap + XHCI_LEGSUP, legsup);
            forced = 1;
        }
    }

    // Disable every SMI source and clear the latched SMI status bits. Without
    // this, SMM can still be woken by the controller we now own. The status
    // bits are RW1C, so writing them back as ones is what clears them.
    uint32_t ctl = xhci_read32(cap + XHCI_LEGCTLSTS);
    ctl &= ~(uint32_t)XHCI_LEGCTL_SMI_ENABLES;
    ctl |= XHCI_LEGCTL_SMI_RW1C;
    xhci_write32(cap + XHCI_LEGCTLSTS, ctl);

    return forced ? XHCI_ERR_BIOS_HANDOFF : XHCI_OK;
    */
    return 0;
}


USBRESULT xhcihcd_halt(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    //Make sure Run/stop bit is clear (STOP)
    uint32_t cmd = mmio_read32(reg_op_mmiobase + XHCI_OP_USBCMD);
    cmd &= ~(uint32_t)XHCI_CMD_RS;
    mmio_write32(reg_op_mmiobase + XHCI_OP_USBCMD, cmd);

    //wait for xhci to set halted bit
    int timeout = 10;
    while ( !(mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_STS_HCH)) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for xhci halt\n");
            return USB_ERROR;
        }
        mdelay(2);
    }
    log_msg("xhci halted!\n");

    return USB_OK;
}

USBRESULT xhcihcd_reset(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    //USBRESULT ret;
    if (xhcihcd_halt(hcd) != USB_OK) {
        return USB_ERROR;
    }

    mmio_write32(reg_op_mmiobase + XHCI_OP_USBCMD, XHCI_CMD_HCRST);
    //wait for xhci to clear both reset bit && controillerNotReady bit

    // HCRST self-clears when the reset completes, and CNR stays set until the
    // controller is ready to accept register writes again. Both must be
    // waited on; checking only one is a common source of "the first write
    // after reset vanished".

    int timeout = 50; //100ms
    while (
            (mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_CMD_HCRST) &&
            (mmio_read32(reg_op_mmiobase + XHCI_OP_USBSTS) & XHCI_STS_CNR) ) {
        if (--timeout==0) {
            log_msg("Timeout while waiting for xhci reset\n");
            return USB_ERROR;
        }
        mdelay(2);
    }
    log_msg("xhci reset OK!\n");

    mdelay(USB_TRSTRCY);

    //check defaults in some reg.

    if (mmio_read32(reg_op_mmiobase + XHCI_OP_USBCMD) != 0) {
        PANIC("xhci CMD reg should read back as 0 after reset!\n");
    }
    if (mmio_read32(reg_op_mmiobase + XHCI_OP_DNCTRL) != 0) {
        PANIC("xhci DNCTRL reg should read back as 0 after reset!\n");
    }
    if (mmio_read32(reg_op_mmiobase + XHCI_OP_CONFIG) != 0) {
        PANIC("xhci CONFIG reg should read back as 0 after reset!\n");
    }
    if (mmio_read64(reg_op_mmiobase + XHCI_OP_CRCR) != 0) {
        PANIC("xhci CRCR reg should read back as 0 after reset!\n");
    }
    if (mmio_read64(reg_op_mmiobase + XHCI_OP_DCBAAP) != 0) {
        PANIC("xhci DCBAAP reg should read back as 0 after reset!\n");
    }
    return USB_OK;
}


int xhcihcd_setup(usb_hcd_t *hcd) {
    xhci_hcd_device_t* xhcidev = (xhci_hcd_device_t*) hcd;
    adr_t mmioregbase = xhcidev->mmio_regbase;
    adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;
    //adr_t reg_op_mmiobase = xhcidev->reg_op_mmiobase;

    //disable bios legacy suport.
    xhci_disable_legacy_support(xhcidev);

    if (xhcihcd_halt(hcd) != USB_OK) {
        PANIC("Failed to halt xhci");
    }

    if (xhcihcd_reset(hcd) != USB_OK) {
        PANIC("Failed to reset xhci");
    }

    uint32_t hcsp1 = mmio_read32(mmioregbase+XHCI_CAP_HCSPARAMS1);
    uint32_t max_slots = XHCI_HCSP1_MAXSLOTS(hcsp1);
    uint32_t max_intrs = XHCI_HCSP1_MAXINTRS(hcsp1);
    uint32_t max_ports = XHCI_HCSP1_MAXPORTS(hcsp1);

    if (max_ports > XHCI_MAX_PORTS_SUPPORTED) {
        max_ports = XHCI_MAX_PORTS_SUPPORTED;
    }
    log_msg("XHCI HCSPARAMS1 register:\n");
    log_msg("  max_slots = %d (%#x)\n",max_slots,max_slots);
    log_msg("  max_intrs = %d (%#x)\n",max_intrs,max_intrs);
    log_msg("  max_ports = %d (%#x)\n",max_ports,max_ports);

    uint32_t pagesizebits = mmio_read32(reg_op_mmiobase + XHCI_OP_PAGESIZE);
    uint32_t pagesize = (pagesizebits & 0xFFFF)<<12;
    log_msg("  pagesize = %d (%#x)\n",pagesize,pagesize);

    PANIC("TODO");
    xhcidev->usb_hcd.hcdstate = HCD_STATE_SETUP;
    return 0;
}

int xhcihcd_start(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}
int xhcihcd_stop(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}
int xhcihcd_release(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}
int xhcihcd_poll(usb_hcd_t *hcd) {
    PANIC("TODO");
    return 0;
}


int xhcihcd_pci_probe(device_t *dev) {
    pcidevice_t* pcidev = (pcidevice_t*) dev;
    log_msg("hcihcd_pci_probe\n");


    xhci_hcd_device_t* priv = kmalloc(sizeof(xhci_hcd_device_t),0);
    dev->priv = priv;

    pci_config_parse_bars(pcidev,MAX_BARS);

    PCILOG(pcidev, "xhcihcd PROBE\n");
    if (pci_enable_device(pcidev)) {
        return -EIO;
    }

    // BAR0 is a 64-bit memory BAR on every xHCI implementation. Probe its
    // size the standard PCI way: write all ones, read back, restore.
    if (pcidev->bar[0].bartype != BARTYPE_MEM) {
        PANIC("XHCI ONLY SUPPORTS MEMORY BAR");
    }


    log_msg("BAR 0 (MEMORY)   = 0x%lx \n",pcidev->bar[0].addr);
    adr_t reg_base;
    // Mappa fysiskt PCI-minne till virtuellt kernel-minne (MMIO)
    reg_base = pci_iomap(pcidev, &pcidev->bar[0], 0);
    if (!reg_base) {
        pci_release_region(pcidev, &pcidev->bar[0]);
        pci_disable_device(pcidev);
        PANIC("UNABLE to map reg_base via pci_iomap");
        return -ENOMEM;
    }
    priv->io_regbase=0;
    priv->mmio_regbase=reg_base; //disable
    adr_t mmioregbase = priv->mmio_regbase;

    //test USB HCD Version från PCI config extended field 0x60
    uint8_t usbversion = pci_config_read8(pcidev,PCICONFIG_USB_EXT_SBRN); //0x60, USB version of UHCI=0x10, EHCI=0x20, xHCI=0x30
    log_msg("usbversion= %#x\n",usbversion);
    if(usbversion!=0x30){
        PANIC("STRANGE USB VERSION FOR XHCI CONTROLLER");
        return ERROR;
    }


    //Enable I/O + bumastering + intx disable
    uint16_t command = pci_config_read16(pcidev,PCICONFIG_COMMAND);
    log_msg("command= %#x\n",command);
    command = command | PCICONFIG_CMD_MEM_SPACE | PCICONFIG_CMD_BUS_MASTER | PCICONFIG_CMD_INTX_DISABLE;  // bit0=I/O Space, bit1=Memory Space, bit2=Bus Master, bit10 = intx disable
    pci_config_write16(pcidev,PCICONFIG_COMMAND,command);

    //test MSI support
    uint32_t capptr_msi = pci_config_get_capabillity_ptr(pcidev,PCICONFIG_CAP_ID_MSI);
    uint32_t capptr_msix = pci_config_get_capabillity_ptr(pcidev,PCICONFIG_CAP_ID_MSIX);
    log_msg("Found MSI Capability PTR = 0x%x\n", capptr_msi);
    log_msg("Found MSIX Capability PTR = 0x%x\n", capptr_msix);
    if (capptr_msi==0 && capptr_msix==0) {
        PANIC("This XHCI Controller has no MSI or MSIX support!");
        /*
        uint8_t msiirq=11+100+32; //100 is MSI IRQ OFSETT
        adr_t message_adr_reg32 = 0x0FEE << 20;
        uint32_t core_id = 0x0;
        message_adr_reg32 = message_adr_reg32 | (core_id << 12);
        pci_config_msi_addirq(pcidev,capptr,msiirq,qemuedu_irqhandler,message_adr_reg32) ;
        */
    }

    // Register block bases. Everything below depends on these.
    //uint8_t caplength = mmio_read8(mmioregbase+XHCI_CAP_CAPLENGTH);
    //uint16_t hci_version = mmio_read16(mmioregbase+XHCI_CAP_HCIVERSION);

    //Workaround for Qemu. (doesnt allow to do reads that arent 32bit aligned), shoule also work in bare metal
    uint32_t cap_reg32 = mmio_read32(mmioregbase+XHCI_CAP_CAPLENGTH);
    uint8_t caplength = cap_reg32 & 0xFF;   //Extract CAPLENGTH (Bits 0-7)
    uint16_t hci_version = (cap_reg32 >> 16) & 0xFFFF;  //Extract HCIVERSION (Bits 16-31)


    adr_t   opregoff = caplength;
    uint16_t rtsoff = mmio_read32(mmioregbase+XHCI_CAP_RTSOFF);
    uint16_t dboff = mmio_read32(mmioregbase+XHCI_CAP_DBOFF);

    log_msg("XHCI CAPABILLITY registers:\n");
    log_msg("  CAPLENGTH = %d (%#x)\n",caplength,caplength);
    log_msg("  HCIVERSION = %d (%#x)\n",hci_version,hci_version);
    log_msg("  OPREGOFF = %d (%#x)\n",opregoff,opregoff);
    priv->reg_op_mmiobase = mmioregbase+opregoff;
    log_msg("  RTSOFF = %d (%#x)\n",rtsoff,rtsoff);
    log_msg("  DBOFF = %d (%#x)\n",dboff,dboff);

    uint32_t hccp1 = mmio_read32(mmioregbase+XHCI_CAP_HCCPARAMS1);
    log_msg("  HCCPARAMS1 = %#x\n",hccp1);
    uint32_t xECPoff =  ((uint32_t)((hccp1 >> 16) & 0xFFFF))*4;
    priv->xECPoff = xECPoff;
    log_msg("  xECPoff = %d,(%#x)\n",xECPoff,xECPoff);

    uint32_t ac64 =  hccp1 & XHCI_HCCP1_AC64;
    uint32_t csz =  hccp1 & XHCI_HCCP1_AC64;
    log_msg("  ac64 = %d,(%#x)\n",ac64,ac64);
    log_msg("  csz = %d,(%#x)   (1=64 bytes, 0=32 bytes)\n",csz,csz);  //1 = 64 byte, 0 = 32 byte

    if (ac64==0) {
        PANIC("TODO implement support for 32bit adressing");
    }
    if (csz==0) {
        PANIC("TODO implement support for 32byte Context Size");
    }

    /*
    uint8_t caplength = xhci_read8(XHCI_CAP_CAPLENGTH);
    g_xhci.hci_version = xhci_read16(XHCI_CAP_HCIVERSION);
    g_xhci.op_base = caplength;
    g_xhci.rt_base = xhci_read32(XHCI_CAP_RTSOFF) & ~0x1Fu;
    g_xhci.db_base = xhci_read32(XHCI_CAP_DBOFF) & ~0x03u;

    uint32_t hccp1 = xhci_read32(XHCI_CAP_HCCPARAMS1);
    g_xhci.xecp = XHCI_HCCP1_XECP(hccp1) * 4u;
    g_xhci.context_size = (hccp1 & XHCI_HCCP1_CSZ) ? 64u : 32u;
    g_xhci.ac64 = (hccp1 & XHCI_HCCP1_AC64) ? 1 : 0;
    */
    //PANIC("TODO");

    priv->usb_hcd.name = "UHCI";
    priv->usb_hcd.setup=xhcihcd_setup;
    priv->usb_hcd.start=xhcihcd_start;
    priv->usb_hcd.stop=xhcihcd_stop;
    priv->usb_hcd.release=xhcihcd_release;
    priv->usb_hcd.poll=xhcihcd_poll;

    //priv->usb_hcd.hcd_roothub_is_port_present = uhcihcd_roothub_is_port_present;
    //priv->usb_hcd.hcd_roothub_port_reset = uhcihcd_roothub_port_reset;
    //priv->usb_hcd.hcd_roothub_is_port_device_present=uhcihcd_roothub_is_port_device_present;
    //priv->usb_hcd.hcd_roothub_port_speed=uhcihcd_roothub_port_speed;

    //priv->usb_hcd.submit_ctrl_xfer_request=uhcihcd_submit_ctrl_xfer_request;
    //priv->usb_hcd.submit_endp_xfer_request_sync=uhcihcd_submit_endp_xfer_request_sync; //TODO, change to interrupt driver
    //priv->usb_hcd.submit_endp_xfer_request=uhcihcd_submit_endp_xfer_request;


    priv->usb_hcd.hcdstate = HCD_STATE_UNDEFINED;

    usb_debug("XHCI PROBED!\n" );



    /*
    //Disable legacy support for Keyboard
    uint32_t legacydefault = pci_config_read32(pcidev,PCICONFIG_USB_EXT_USBLEGSUP);
    log_msg("Legacy support = %#x\n",legacydefault);
    pci_config_write16(pcidev,PCICONFIG_USB_EXT_USBLEGSUP,0x8F00);  // disable all SMI sources, clear their status

    if(! (uhcihcd_global_reset(ioregbase)==OK)){
        PANIC("Unable to reset UHCI Controller?\n");
    }

    if(! (uhcihcd_setup_schedule(dev)==OK)){
        PANIC("Unable to setup UHCI Schedule\n");
    }

    // set the Host Controllers schedule
    io_outd(ioregbase+UHCI_REG_FRBASEADD, V2P(priv->stackframe)); // physical address
    io_outw(ioregbase+UHCI_REG_FRNUM, 0);                // start at frame 0
    io_outb(ioregbase+UHCI_REG_SOFMOD, 0x40);            // start of frame to default,standard 12MHz SOF timing value
    io_outw(ioregbase+UHCI_REG_USBINTR, 0x0000);         // no interrupts, means polling
    io_outw(ioregbase+UHCI_REG_USBSTS, 0xFFFF);              // Clear any status bits.

    //start controller
    //bit0 (RS) = Run/Stop=1 (execute the frame list),
    //bit6 (CF)= Configure Flag=1 (informs the controller/root hub that software has finished configuring it — required by spec before ports will function normally)
    //bit7 (MAXP) = Max Packet=1 (allow 64-byte packets; harmless for low-speed/8-byte-max devices, needed for full-speed ones)
    io_outw(ioregbase+UHCI_REG_USBCMD, (1<<7) | (1<<6) | (1<<0));


    priv->usb_hcd.name = "UHCI";
    priv->usb_hcd.hcd_roothub_is_port_present = uhcihcd_roothub_is_port_present;
    priv->usb_hcd.hcd_roothub_port_reset = uhcihcd_roothub_port_reset;
    priv->usb_hcd.hcd_roothub_is_port_device_present=uhcihcd_roothub_is_port_device_present;
    priv->usb_hcd.hcd_roothub_port_speed=uhcihcd_roothub_port_speed;

    priv->usb_hcd.submit_ctrl_xfer_request=uhcihcd_submit_ctrl_xfer_request;
    priv->usb_hcd.submit_endp_xfer_request_sync=uhcihcd_submit_endp_xfer_request_sync; //TODO, change to interrupt driver
    priv->usb_hcd.submit_endp_xfer_request=uhcihcd_submit_endp_xfer_request;
    priv->usb_hcd.poll=uhcihcd_poll;

    //usb_attach_hcd(&priv->usb_hcd);
    usb_debug("UHCI STARTED!\n" );




    */
    dev->initialized = 1;
    return 0;
}

int xhcihcd_pci_attach(device_t *dev) {
    xhci_hcd_device_t* priv = dev->priv;
    if (dev->initialized) {
        log_msg("xhcihcd_pci_attach\n");
        priv->usb_hcd.hcdstate = HCD_STATE_INIT;
        usb_attach_hcd(&priv->usb_hcd);
    }else {
        log_msg("Device is not initialized, skip attach\n");
    }

    return 0;
}

struct pcidriver xhcihcd_pci_driver = {
    .driver.name = "USB Universal Host Controller Interface (XHCI)",
    .driver.handleflags = DRIVER_PCI,
    //.init = gd5446_pci_init,
    .driver.probe = xhcihcd_pci_probe,
    .driver.attach = xhcihcd_pci_attach,
    .idents = (void *) &xhcihcd_pci_idents,
};

REGISTER_DRIVER(xhcihcddriver, "xhcihcd",&xhcihcd_pci_driver);

