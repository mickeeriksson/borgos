#ifndef _HPET_H_
#define _HPET_H_

#include "types.h"



struct hpetinfo {
    adr_t base;

};

extern void hpet_init(struct hpetinfo* hpet) ;
extern void hpet_debug_regs(struct hpetinfo *hpet);

extern uint64_t hpet_vsystimer_get_millis(void* hpetptr) ;
#endif