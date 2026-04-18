#include "types.h"
#include "config.h"
#include "cpu.h"
//#include "cpu/mmio.h"
//#include "delay.h"
//#include "error.h"


#ifdef CONFIG_APIC
cpu_t* apic_id_cpumap[MAXAPIC_LOGICAL_CPUID];



#endif // CONFIG_APIC