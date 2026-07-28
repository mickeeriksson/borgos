#ifndef _DRIVER_VIDEO_BOCHSVBE_H_
#define _DRIVER_VIDEO_BOCHSVBE_H_

#include "types.h"
#include "cpu/mmio.h"
#include "cpu/io.h"

extern uint16_t bochsvbe_getversion(adr_t regbase) ;
extern void bochsvbe_disable(adr_t regbase);
extern void bochsvbe_set_mode(adr_t regbase,uint16_t width, uint16_t height, uint16_t bpp);

#endif