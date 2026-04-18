#ifndef __CPU_x86_IO_H_
#define __CPU_x86_IO_H_

#include "cpu/cpu_types.h"

/* Input a byte from a port */
static inline unsigned char io_inb(uint32_t port)
{
    unsigned char ret;
    __asm__ volatile ("inb %%dx,%%al":"=a" (ret):"d" (port));
    return ret;
}

/* Input a word from a port */
static inline uint16_t io_inw(uint32_t port)
{
    uint16_t ret;
    __asm__ volatile ("inw %%dx,%%ax":"=a" (ret):"d" (port));
    return ret;
}

/* Input a double word from a port */
static inline uint32_t io_ind(uint32_t port)
{
    uint32_t ret;
    __asm__ volatile ("in %%dx,%%eax":"=a" (ret):"d" (port));
    return ret;
}

/* Write a byte to a port */
static inline void io_outb(uint32_t port,uint8_t value)
{
    __asm__ volatile ("outb %%al,%%dx": :"d" (port), "a" (value));
}

/* Write a word to a port */
static inline void io_outw(uint32_t port,uint16_t value)
{
    __asm__ volatile ("outw %%ax,%%dx": :"d" (port), "a" (value));
}

/* Write a double word (32bit) to a port */
static inline void io_outd(uint32_t port,uint32_t value)
{
    __asm__ volatile ("out %%eax,%%dx": :"d" (port), "a" (value));
}


#endif
