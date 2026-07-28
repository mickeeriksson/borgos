#include "log.h"
#include "types.h"
#include "vector.hpp"
#include "driver/pci/pci_config_region.hpp"
#include "mm.h"
#include "bits.h"
#include "driver/pci/pci.h"
#include "error.h"
#include "hwdb.h"
#include "../../include/device.h"
#include "../../include/mm.h"
#include "cpu/mmio.h"

//Använd inte en ny template då detta skapar upp kopior av klassen Vector
//template class Vector<PciConfigRegion*>;

//Vector<PciConfigRegion*> *vecRegions;
Vector<void*> *vecRegions;

/// The address of a PCIe function.
///
/// PCIe supports 65536 segments, each with 256 buses, each with 32 slots, each with 8 possible functions. We pack this into a `u32`:
///
/// ```ignore
/// 32                              16               8         3      0
///  +-------------------------------+---------------+---------+------+
///  |            segment            |      bus      | device  | func |
///  +-------------------------------+---------------+---------+------+
/// ```
extern "C" adr_t _pci_location_to_ecam_offset(pci_location_t pciloc) {
    return (   (((adr_t) pciloc.bus) << 20) + (((adr_t)pciloc.slot ) << 15) + (((adr_t) pciloc.function) << 12) );
}

extern "C" pci_config_regs_t* _pci_config_regs_from_ecamaddr(adr_t region_base_address,pci_location_t pciloc) {
    adr_t offsett = _pci_location_to_ecam_offset(pciloc);
    adr_t regadr = region_base_address + offsett;
    //log_msg("  calc pci_config_regs from 0x%lx\n", regadr);
    return (pci_config_regs_t*) P2V(regadr);
}

extern "C" void pci_config_init(void) {
    log_msg("pci_config_init\n");
    vecRegions = new Vector<void*>();
}



extern "C" void pci_config_add_region(uint64_t base_address,uint16_t pci_segment_group,uint8_t bus_number_start,uint8_t bus_number_end) {
    log_msg("pci_config_add_region\n");
    log_msg("  base=0x%lx\n", base_address);
    log_msg("  pci_segment_group=%d\n", pci_segment_group);
    log_msg("  bus_number_start=%d\n", bus_number_start);
    log_msg("  bus_number_end=%d\n", bus_number_end);
    PciConfigRegion* pciConfigRegion = new PciConfigRegion(base_address,pci_segment_group,bus_number_start,bus_number_end);
    vecRegions->push_back(pciConfigRegion);

    //memorymap the region
    size_t size = 256*32*8*4096;
    adr_t mapend = (base_address+size);
    adr_t mapadr = base_address;
    while (mapadr < mapend) {
        mmu_map_kernelpage(mapadr,P2V(mapadr),MMU_DEVICE);
        mapadr += PAGESIZE;
    }

    log_msg("  Created pciConfigRegion = 0x%lx\n", pciConfigRegion);
}


extern "C" void pci_config_debugprint_regions(void) {
    log_msg("------------ PCI REGIONS -----------------\n");

    for (int i=0;i<vecRegions->size();i++) {
        PciConfigRegion* pciConfigRegion = (PciConfigRegion*) vecRegions->at(i);
        log_msg("PciConfigRegion ptr=0x%lx\n", pciConfigRegion);
        log_msg("  base=0x%lx\n", pciConfigRegion->base_address);
        log_msg("  pci_segment_group=%d\n", pciConfigRegion->pci_segment_group);
        log_msg("  bus_number_start=%d\n", pciConfigRegion->bus_number_start);
        log_msg("  bus_number_end=%d\n", pciConfigRegion->bus_number_end);
    }

}

extern "C" void pci_config_enumerate_function(PciConfigRegion* pciConfigRegion,uint16_t pci_segment_group,uint8_t bus,uint8_t slot,uint8_t function) {
    log_msg("Enumerate PCI Function\n");
    pci_location_t pciloc = {pci_segment_group,bus,slot,function};
    pci_config_regs_t* regs = _pci_config_regs_from_ecamaddr(pciConfigRegion->base_address,pciloc);
    if (regs->vendor_id == 0xffff) {
        return;
    }else {
        uint8_t type = regs->header_type & 0x7F;
        switch (type) {
            case 0x00: {
                log_msg("Found PCI Device at %04x:%02x:%02x.%x vendor/device=%04x:%04x \n",
                pciloc.segmentgroup,pciloc.bus,pciloc.slot,pciloc.function,
                regs->vendor_id,regs->device_id);

                pcidevice_t* pcidev = (pcidevice_t*) kmalloc(sizeof(pcidevice_t),0);
                pcidev->devicebase.bustype=DEVICE_BUSTYPE_PCIE;
                pcidev->location = pciloc;
                pcidev->ecam_config_reg_adr = (adr_t) regs;
                hwdb_devicedb_add((device_t*)pcidev);
                break;
            }
            case 0x01:
                log_msg("PCI-PCI bridge unsupported!\n", type);
                break;
            case 0x02:
                log_msg("PCI-Cardbus bridge unsupported!\n", type);
                break;
            default:
                log_msg("Unexpected PCI header type: 0x%x\n", type);
                PANIC("Unexpected PCI header type");
                break;
        }

    }
}


extern "C" void pci_config_enumerate_slot(PciConfigRegion* pciConfigRegion,uint16_t pci_segment_group,uint8_t bus,uint8_t slot) {
    pci_location_t pciloc = {pci_segment_group,bus,slot,0};
    pci_config_regs_t* regs = _pci_config_regs_from_ecamaddr(pciConfigRegion->base_address,pciloc);
    //log_msg("  regs=0x%lx\n", regs);
    uint16_t vendorid = regs->vendor_id;
    if (vendorid == 0xffff) {
        return;
    }else {
        log_msg("Found Vendor ID: 0x%x\n", vendorid);
        pci_config_enumerate_function(pciConfigRegion,pci_segment_group,bus,slot,0);
        if ((regs->header_type & 0x80) != 0) {
            //multifunction device, chedck remaining functions
            for (int func=1;func<8;func++) {
                pci_config_enumerate_function(pciConfigRegion,pci_segment_group,bus,slot,func);
            }
        }
    }

}

extern "C" void pci_config_enumerate_regions(void) {
    log_msg("Enumerate PCI Regions\n");

    for (int i=0;i<vecRegions->size();i++) {
        PciConfigRegion* pciConfigRegion = (PciConfigRegion*) vecRegions->at(i);
        for (int busidx=pciConfigRegion->bus_number_start; busidx <= pciConfigRegion->bus_number_end; busidx++) {
            //log_msg("  busidx=%d\n", busidx);
            for (int slotidx=0; slotidx<32;slotidx++) {
                //log_msg("  slotidx=%d\n", slotidx);
                pci_config_enumerate_slot(pciConfigRegion,pciConfigRegion->pci_segment_group, busidx,slotidx);
            }
        }
    }
}

extern "C" int _pcidriverhandlesident(pcidriver_t* pcidriver,uint16_t vendor,uint16_t device) {
    for(int idi=0;pcidriver->idents[idi].vendor != 0xFFFF;idi++){
        //kprintf("TEST Driver  %#x : %#x\n",pci_drivers[i]->idents[idi].vendor,pci_drivers[i]->idents[idi].device);
        if((pcidriver->idents[idi].vendor == vendor) && (pcidriver->idents[idi].device == device)) {
            //kprintf("MATCH for %s\n",pcidriver->idents[idi].name);
            log_msg("FOUND DRIVER MATCH for %s\n",pcidriver->idents[idi].name);
            return 0;
        }
    }
    return -1;
}

extern "C" uint8_t pci_config_read8(pcidevice_t* pcidev,adr_t offsett) {
    if (pcidev->ecam_config_reg_adr != NULL) {
        //read ecam
        return mmio_read8(pcidev->ecam_config_reg_adr+offsett);
    }else {
        //read old style not supported
        PANIC("pci_config_read8 no ECAM adr! read old style not supported");
    }
    return 0;
}
extern "C" void pci_config_write8(pcidevice_t* pcidev,adr_t offsett,uint8_t data) {
    if (pcidev->ecam_config_reg_adr != NULL) {
        //read ecam
        mmio_write8(pcidev->ecam_config_reg_adr+offsett,data);
    }else {
        //read old style not supported
        PANIC("pci_config_write8 no ECAM adr! read old style not supported");
    }
}

extern "C" uint16_t pci_config_read16(pcidevice_t* pcidev,adr_t offsett) {
    if (pcidev->ecam_config_reg_adr != NULL) {
        //read ecam
        return mmio_read16(pcidev->ecam_config_reg_adr+offsett);
    }else {
        //read old style not supported
        PANIC("pci_config_read16 no ECAM adr! read old styöle not supported");
    }
    return 0;
}
extern "C" void pci_config_write16(pcidevice_t* pcidev,adr_t offsett,uint16_t data) {
    if (pcidev->ecam_config_reg_adr != NULL) {
        //read ecam
        mmio_write16(pcidev->ecam_config_reg_adr+offsett,data);
    }else {
        //read old style not supported
        PANIC("pci_config_write16 no ECAM adr! read old styöle not supported");
    }
}

extern "C" uint32_t pci_config_read32(pcidevice_t* pcidev,adr_t offsett) {
    if (pcidev->ecam_config_reg_adr != NULL) {
        //read ecam
        return mmio_read32(pcidev->ecam_config_reg_adr+offsett);
    }else {
        //read old style not supported
        PANIC("pci_config_read32 no ECAM adr! read old styöle not supported");
    }
    return 0;
}
extern "C" void pci_config_write32(pcidevice_t* pcidev,adr_t offsett,uint32_t data) {
    if (pcidev->ecam_config_reg_adr != NULL) {
        //read ecam
        mmio_write32(pcidev->ecam_config_reg_adr+offsett,data);
    }else {
        //read old style not supported
        PANIC("pci_config_write32 no ECAM adr! read old styöle not supported");
    }
}

extern "C" void pci_config_parse_bars(pcidevice_t* pcidev,int maxbars) {
    for (int baridx=0;baridx<maxbars;baridx++) {
        uint32_t baroffsett = 0x10+(baridx*4);
        uint32_t bardata = pci_config_read32(pcidev,baroffsett);
        adr_t base=0;
        uint32_t size=0;
        enum bartype_t stype=BARTYPE_UNKNOWN;
        enum barwidth_t swidth=BARWIDTH_UNKNOWN;
        uint8_t prefetchable=0; //1 if prefetchable
        log_msg("    GOT BARDATA (%d) = #%x\n",baridx,bardata);

        if(bardata & 0x01) {
            log_msg("      IS I/O BAR\n");
            base = bardata & ~0b11; //filter out bit 0:1
            stype=BARTYPE_IO;
        }else if(bardata == 0) {
            log_msg("      IS UNUSED BAR\n");
        }else{
            log_msg("      IS MEMORY BAR\n");
            //type=0;
            stype=BARTYPE_MEM;
            uint32_t mtype = (bardata & 0x06) >> 1;
            if(mtype == 0){
                //32 bit
                log_msg("      IS 32 bit MEMORY BAR\n");
                swidth=BARWIDTH_32;
                pci_config_write32(pcidev,baroffsett,0xFFFFFFFF);
                uint32_t readbacklow = pci_config_read32(pcidev,baroffsett);
                size = readbacklow & ~0xF; //maskera bort kontrollbitarna
                size = ~size + 1; //invertera och lägg till 1
                pci_config_write32(pcidev,baroffsett,bardata);
                log_msg("      Size=0x%lx (%d)\n",size,size);
            }
            if(mtype == 1){
                //???
                log_msg("      IS ??????MEMORY BAR\n");
                swidth=BARWIDTH_UNKNOWN;
            }
            if(mtype == 2){
                //64 bit
                log_msg("      IS 64 bit MEMORY BAR\n");
                swidth=BARWIDTH_64;
                PANIC("IS 64bit BAR, TODO handle FETCH Additional registers...");
            }
            if(bardata & (1<<3)){
                //bit 3 means prefetchable.
                log_msg("      IS PREFETCHABLE\n");
                prefetchable=1;
            }
            base = bardata & ~0b1111; //filter out bit 0:3
        }
        pcidev->bar[baridx].addr = base;
        pcidev->bar[baridx].size = size;
        pcidev->bar[baridx].bartype = stype;
        pcidev->bar[baridx].barwidth = swidth;
        pcidev->bar[baridx].prefetchable = prefetchable;

        //log_msg("Bar %d\n",baridx);
    }
}

extern "C" uint32_t pci_config_get_capabillity_ptr(pcidevice_t* pcidev,uint8_t id) {
    uint32_t rawdata = pci_config_read32(pcidev,0x04); //command + status
    uint16_t status = rawdata >> 16;
    uint32_t hascap = bits_32_get(status,4,4);
    if (!hascap ) {
        return 0;
    }
    rawdata = pci_config_read32(pcidev,0x34); //cap pointer
    uint32_t capptr =  bits_32_get(rawdata,0,7);
    log_msg("Capabillity pointer = 0x%x (%d)\n",capptr,capptr);

    while (capptr > 0 ) {
        rawdata = pci_config_read32(pcidev,capptr);
        uint32_t cid = bits_32_get(rawdata,0,7);
        log_msg("Capabillity id = 0x%x (%d)\n",cid,cid);
        if (cid==id) {
            log_msg("Found capabillity 0x%x\n",cid);
            return capptr;
        }
        capptr =  bits_32_get(rawdata,8,15); //next pointer
    }
    return 0;
}

extern "C" void pci_config_msi_addirq(pcidevice_t* pcidev,uint32_t capptr,uint8_t msiirq,void* irqhandler,adr_t message_adr_reg32) {
    pci_config_write32(pcidev,capptr+0x04,message_adr_reg32); //message_address_register_32 = message_adr_reg32

    //set message data
    uint32_t mcr = pci_config_read16(pcidev,capptr+0x02); //mcr
    //uint32_t mcr = bits_32_get(rawdata,16,31);
    if (bits_32_get(mcr,7,7)) {
        //64bit
        uint32_t msgdata = pci_config_read16(pcidev,capptr+0x0C) & 0xFFFF;
        log_msg("Current Message data = 0x%x, raplace with 0x%x\n",msgdata,msiirq);
        pci_config_write16(pcidev,capptr+0x0C,msiirq);
    }else {
        //32bit
        uint32_t msgdata = pci_config_read16(pcidev,capptr+0x08) & 0xFFFF;
        log_msg("Current Message data = 0x%x, raplace with 0x%x\n",msgdata,msiirq);
        pci_config_write16(pcidev,capptr+0x08,msiirq);
    }

    //add irq handler

    //enable
    mcr = bits_32_set(mcr,1,0,0);  //enable by setting bit 0=1
    pci_config_write16(pcidev,capptr+0x02,mcr);

    // OBS: Glöm inte att också stänga av INTx-avbrott i PCI Command-registret (offset 0x04)
    uint16_t pci_command = pci_config_read16(pcidev, 0x04);
    pci_command |= (1 << 10); // Bit 10: Interrupt Disable (Stänger av äldre INTx-linor)
    pci_config_write16(pcidev, 0x04, pci_command);
}


extern "C" void pci_config_driver_init(void) {
    log_msg("pci_config_driver_init\n");

    //get ALL DRIVERS
    log_msg("Get all pci devices\n");
    Vector<void*> *devvector = new Vector<void*>();
    hwdb_devicedb_getbytype(devvector, DEVICE_BUSTYPE_PCIE);
    log_msg("---------------- PCI DEVICE LIST TO MATCH --------------------d\n");
    for (int i = 0; i < devvector->size(); i++) {
        device_t* device = (device_t*) devvector->at(i);
                pcidevice_t* pcidev = (pcidevice_t*) device;
                pci_location_t* pciloc = &pcidev->location;
                pci_config_regs_t* regs = (pci_config_regs_t*) pcidev->ecam_config_reg_adr;
                log_msg("PCI Device at %04x:%02x:%02x.%x vendor/device=%04x:%04x class=%02x:%02x (%s)\n",
                pciloc->segmentgroup,pciloc->bus,pciloc->slot,pciloc->function,
                regs->vendor_id,regs->device_id,
                regs->class_code,regs->subclass,
                pci_class_typename(regs->class_code,regs->subclass,regs->prog_if)
                );
    }

    log_msg("\n\nGet all drivers\n");
    Vector<void*> *drivervector = new Vector<void*>();
    hwdb_driverdb_getbytype(drivervector, DRIVER_PCI);
    log_msg("---------------- PCI DRIVER LIST TO MATCH --------------------d\n");
    for (int i = 0; i < drivervector->size(); i++) {
        driver_t* driver = (driver_t*) drivervector->at(i);
        log_msg("PCI Driver : %s\n",driver->name);
    }

    for (int i = 0; i < devvector->size(); i++) {
        device_t* device = (device_t*) devvector->at(i);
        pcidevice_t* pcidev = (pcidevice_t*) device;
        pci_location_t* pciloc = &pcidev->location;
        pci_config_regs_t* regs = (pci_config_regs_t*) pcidev->ecam_config_reg_adr;
        uint16_t vendorid = regs->vendor_id;
        uint16_t deviceid = regs->device_id;


        for (int i = 0; i < drivervector->size(); i++) {
            driver_t* driver = (driver_t*) drivervector->at(i);
            pcidriver_t* pcidriver = (pcidriver_t*) driver;
            if (_pcidriverhandlesident(pcidriver, vendorid, deviceid) == 0 ) {
                log_msg("---------------------------------------------\n");
                log_msg("- START DEVICE at %04x:%02x:%02x.%x class=%02x:%02x (%s), with DRIVER: %s\n",
                pciloc->segmentgroup,pciloc->bus,pciloc->slot,pciloc->function,
                regs->class_code,regs->subclass,
                pci_class_typename(regs->class_code,regs->subclass,regs->prog_if),
                driver->name);

                //Enable mem + i/o + busmaster
                uint16_t cmdreg = pci_config_read16(pcidev,0x04) & 0xFFFF; //cmd reg
                pci_config_write16(pcidev,0x04,cmdreg | 0x3 | 0x04);
                cmdreg = pci_config_read16(pcidev,0x04) & 0xFFFF; //cmd reg
                log_msg("Command = %#x\n",cmdreg);

                log_msg("Probe (& bind) device");
                pcidriver->driver.probe((device_t*) pcidev);
                log_msg("Attach (& start) device");
                pcidriver->driver.attach((device_t*) pcidev);
                log_msg("---------------------------------------------\n");

            }
        }

    }


    log_msg("\n\nDONE\n");

    //og_msg("\n\nDelete devicevector\n");
    delete devvector;
    //log_msg("\n\nDelete drivervector\n");
    delete drivervector;

    log_msg("pci_config_driver_init DONE\n\n\n");
    //PANIC("TODO");
}


