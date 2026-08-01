#ifndef _CPU_ARCHCPU_CONTEXT_H_
#define _CPU_ARCHCPU_CONTEXT_H_

#include "types.h"
#include "log.h"

typedef struct arch_cpu_context {
    //context for proc, used in context_switch, saved in proc_t
    adr_t sp;
    adr_t cr3;  //saved page table
} arch_cpu_context_t;


#endif