#ifndef _DRIVER_H
#define _DRIVER_H

#include "device.h"

#define DRIVER_ISA 0x01  //Hardwired to an address
#define DRIVER_PCI 0x02  //Hardwired to an address
#define DRIVER_USB 0x04  //Hardwired to an address

typedef struct driver {
    char *name;
    uint16_t handleflags;
    // probe tests if a device is present (by using the matched driver)
    int (*probe)(device_t *);
    // attach will setup the device, and connect it to the hardware.
    int (*attach)(device_t *);
} driver_t;

//Driver Annotation
typedef struct {
    const char *name;
    //int id;
    //void (*init_func)(void);
    void* driver;
} driver_annotation_t;

// Makro för att förenkla annoteringen och tvinga kompilatorn att behålla koden
#define REGISTER_DRIVER(obj_name, mod_name,driver_ptr) \
static const driver_annotation_t obj_name __attribute__((section("driver_annotation"), used)) = { \
.name = mod_name, \
.driver = driver_ptr, \
}

#endif