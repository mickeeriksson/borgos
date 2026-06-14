#ifndef _CPU_CPUTIMER_H_
#define _CPU_CPUTIMER_H_

extern void cputimer_initcpu_hz(void);
extern void cputimer_delayticks(uint64_t ticks);
extern void cputimer_mdelay(uint64_t millis);

#endif
