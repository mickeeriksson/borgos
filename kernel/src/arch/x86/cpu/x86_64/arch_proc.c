#include "log.h"
#include "cpu.h"
#include "proc.h"
#include "mm.h"


extern void cpu_kerneltask_trampoline(void);

void archproc_prepare_kernelproc_stack(proc_t* p, void (*entry)(void *), void *arg) {
    log_msg("prepare_kernelproc_stack\n");
    
    uint64_t *sp = (uint64_t *)(p->kstackaddr + p->kstacksize-64);

    // /* KRITISKT: se stack-alignment-avsnittet nedan innan du hoppar över detta
    //sp = (uint64_t *)((uintptr_t)sp & ~0xFULL);   // tvinga 16-aligned bas

    //sp = (uint64_t *)((uint8_t *)sp - 8);          // justera till 8 mod 16
    sp = (uint64_t *)((uint8_t *)sp - 0);          // justera till 8 mod 16

    *(--sp) = (uint64_t)cpu_kerneltask_trampoline;  // "returadress" som ret hoppar till
    *(--sp) = 0;                          // rbp
    *(--sp) = 0;                          // rbx
    *(--sp) = (uint64_t)entry;            // r12 — smugglad entry-pekare
    *(--sp) = (uint64_t)arg;              // r13 — smugglat argument
    *(--sp) = 0;                          // r14
    *(--sp) = 0;                          // r15
    //*(--sp) = 0x202;                      // rflags, IF=1
    *(--sp) = 0x002;                      // rflags, IF=0, Dont set Irq on in trampoline. It will be set in finish_switch. in spinunlock.

    p->ncli = 1; //need to set ncli to 1 since in will do ncli-- in spinunlock (from finish_switch)
    p->intenable = 1;

    //p->rsp = (uint64_t)sp;
    p->archcpu_context.sp =  (uint64_t) sp;
    p->archcpu_context.cr3 = (adr_t) V2P(p->pgdir);


}