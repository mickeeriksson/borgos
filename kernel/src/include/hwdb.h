#ifndef _HWDB_H_
#define _HWDB_H_

#include "device.h"
#include "driver/driver.h"

#ifdef __cplusplus
extern "C" {
#endif


extern void hwdb_devicedb_init(void);
extern void hwdb_driverdb_init(void);
extern void hwdb_devicedb_add(device_t* device);
extern void hwdb_driverdb_add(driver_t* driver);
extern void hwdb_devicedb_debug_listdevices(void);

extern void hwdb_devicedb_getbytype(void* retvector, uint8_t devicetype);
extern void hwdb_driverdb_getbytype(void* retvector, uint16_t handleflags);

#ifdef __cplusplus
}
#endif

#endif