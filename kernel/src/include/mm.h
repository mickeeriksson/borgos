#ifndef _MM_H_
#define _MM_H_

#include "config.h"

#ifdef NOMMU
  #define P2V(p) ((adr_t)(p))
  #define V2P(v) ((adr_t)(v))
#else
  #define P2V(p) ((adr_t)(p) + (KERNEL_UPPER_HALF_OFFSET))
  #define V2P(v) ((adr_t)(v) - (KERNEL_UPPER_HALF_OFFSET))
#endif

#define PAGESIZE   (1UL << PAGESHIFT)
#define PAGEMASK   (~(PAGESIZE-1))


/*
 * Round UP & DOWN in power of 2
 */
static inline adr_t PAGEALIGN_UP (adr_t base)
{
    //adr_t alignmask = ~(PAGESIZE-1);
    return (base+PAGESIZE-1) & PAGEMASK;
}
static inline adr_t PAGEALIGN_DOWN (adr_t base)
{
    //adr_t alignmask = ~(PAGESIZE-1);
    return (base) & PAGEMASK;
}

#endif