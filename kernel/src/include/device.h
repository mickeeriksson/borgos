#ifndef _DEVICE_H
#define _DEVICE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DEVICE_BUSTYPE_PCIE 0x01 // PCI-E device, with ecam config support

typedef struct device {
    uint8_t bustype;
    char* name;
    int initialized;
    void* priv;  //private internal devicedata
} device_t;

//typedef struct busdevice {
//} busdevice_t;


#ifdef __cplusplus
}
#endif


#endif