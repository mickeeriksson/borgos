#include "log.h"
#include <vector.hpp>
#include "device.h"
#include "mm.h"
#include "driver/pci/pci.h"

//Hardware and driver database


typedef struct hwdb_device_entry {
    device_t* device;
} hwdb_device_entry_t;


typedef struct hwdb_driver_entry {
    driver_t* driver;
} hwdb_driver_entry_t;

Vector<void*> *devicevector;
Vector<void*> *drivervector;


extern "C" void hwdb_devicedb_init(void) {
    log_msg("\n\nINIT hwdb_devicedb_init\n");

    devicevector = new Vector<void*>();
    log_msg("DONE INIT hwdb_devicedb_init\n");
}

extern "C" void hwdb_driverdb_init(void) {
    log_msg("\n\nINIT hwdb_driverdb_init\n");
    drivervector = new Vector<void*>();
    log_msg("DONE INIT hwdb_driverdb_init\n");

}


extern "C" void hwdb_devicedb_add(device_t* device) {
    log_msg("hwdb_devicedb_add\n");
    hwdb_device_entry_t* entry = (hwdb_device_entry_t*) kmalloc(sizeof(hwdb_device_entry_t),0);
    entry->device = device;
    devicevector->push_back(entry);
}

extern "C" void hwdb_devicedb_getbytype(Vector<void*> *retvector, uint8_t devicetype) {
    log_msg("hwdb getdevice by type\n");

    //Vector<void*> *retvector = new Vector<void*>();
    for (int i = 0; i < devicevector->size(); i++) {
        hwdb_device_entry_t* entry = (hwdb_device_entry_t*) devicevector->at(i);
        device_t* device = entry->device;
        if (device->bustype == DEVICE_BUSTYPE_PCIE) {
            retvector->push_back(device);
        }
    }
}

extern "C" void hwdb_driverdb_getbytype(Vector<void*> *retvector, uint16_t handleflags) {
    log_msg("hwdb getdriver by type\n");
    for (int i = 0; i < drivervector->size(); i++) {
        hwdb_driver_entry_t* entry = (hwdb_driver_entry_t*) drivervector->at(i);
        driver_t* driver = entry->driver;
        if ((driver->handleflags & handleflags) > 0) {
            retvector->push_back(driver);
        }
    }
}

extern "C" void hwdb_driverdb_add(driver_t* driver) {
    log_msg("hwdb_driverdb_add : %s\n",driver->name);
    hwdb_driver_entry_t* entry = (hwdb_driver_entry_t*) kmalloc(sizeof(hwdb_driver_entry_t),0);
    entry->driver = driver;
    drivervector->push_back(entry);
}

extern "C" void hwdb_devicedb_debug_listdevices(void) {
    log_msg("---------------- DEBUG DEVICE LIST --------------------d\n");
    for (int i = 0; i < devicevector->size(); i++) {
        hwdb_device_entry_t* entry = (hwdb_device_entry_t*) devicevector->at(i);
        device_t* device = entry->device;
        switch (device->bustype) {
            case DEVICE_BUSTYPE_PCIE: {
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
        }
    }
}

