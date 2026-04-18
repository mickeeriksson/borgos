#ifndef _CPU_H_
#define _CPU_H_

#include "config.h"
#include "cpu/cpu_cpu.h"
#include "types.h"
//#include "proc.h"

#define CPU_STATUS_HALTED    0;
#define CPU_STATUS_BOOTING   1;
#define CPU_STATUS_UP        2;         //UP but not yet scheduling
#define CPU_STATUS_ACTIVE    3;         //Active AND scheduling

extern void cpu_init_cpustate(int cpun);


// Per-CPU state
typedef struct cpustate {
    uint8_t cpuid;              //os id, cn be different to acpi or apic id. index to cpu[]-array
//    //uchar apicid;                // Local APIC ID
//    //struct context *context;     // Switch here to enter scheduler
//    //struct tss_entry_struct tss;         // Used by x86 to find stack for interrupt
//    //struct segdesc gdt[NSEGS];   // x86 global descriptor table
//    //volatile uint booted;        // Has the CPU started?
    uint8_t status;                  // 0=HALTED, 1=BOOTING, 3=UP,
    int32_t ncli;                    // Depth of pushcli nesting.
    int intenable;              // Were interrupts enabled before pushcli?
//    int preemptenabled;            //should current process be preempted on next entry to userspace?
    int preemptflag;               //should current process be preempted on next entry to userspace?
//
//    //void *tls[2];
//    proc_t *currentproc;
//    proc_t *idleproc;               //idleproc for this cpu.
//    reg_t ticks;                 //increments by local timer
    struct arch_cpu_info archcpu;

//    adr_t stacktop;                 //top of stack
//    adr_t stacksize;                //size of stack (in bytes)
} cpu_t;


//extern cpu_t cpu[NCPU];
extern cpu_t cpu[];
extern uint32_t cpu_numcores;   //count of detected cores
//extern cpu_t *current_cpu;

//#define CURRENTCPU (&cpu[cpu_get_cpuid()])


/* in cpu_inc.S */
//extern int get_cpu_id(void);
//extern int cpu_get_coreid();

/* in irq.c */
//extern void pushcli(void);
//extern void popcli(void);
extern void irq_save(void);
extern void irq_restore(void);



#endif