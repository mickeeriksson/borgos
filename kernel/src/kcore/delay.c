#include "cpu/cpu_getcpu.h"
#include "cpu/cputimer.h"
#include "log.h"


void mdelay(size_t millis) {
    cputimer_mdelay(millis);
}


void delay_initcpu(void) {
    int cpuid = arch_getcpu_cpuid();
    log_msg("delay_init for current CPU coreID=%d\n",cpuid);
    log_msg("CPU(%d) Calibrate cputimer!\n",cpuid);
    cputimer_initcpu_hz();
}