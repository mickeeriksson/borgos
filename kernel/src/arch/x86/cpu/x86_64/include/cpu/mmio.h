#ifndef __CPU_MMIO_H_
#define __CPU_MMIO_H_

#include "types.h"

#define mmio_read8(addr)         (*((volatile uint8_t *)(addr)))
#define mmio_write8(addr, v)     (*((volatile uint8_t *)(addr)) = (uint8_t)(v))


#define mmio_read16(addr)         (*((volatile uint16_t *)(addr)))
#define mmio_write16(addr, v)     (*((volatile uint16_t *)(addr)) = (uint16_t)(v))

#define mmio_read32(addr)         (*((volatile uint32_t *)(addr)))
#define mmio_write32(addr, v)     (*((volatile uint32_t *)(addr)) = (uint32_t)(v))
#define mmio_write32or(addr, v)   (*((volatile uint32_t *)(addr)) |= (uint32_t)(v))

#define mmio_read64(addr)         (*((volatile uint64_t *)(addr)))
#define mmio_write64(addr, v)     (*((volatile uint64_t *)(addr)) = (uint64_t)(v))
#define mmio_write64or(addr, v)   (*((volatile uint64_t *)(addr)) |= (uint64_t)(v))


#endif