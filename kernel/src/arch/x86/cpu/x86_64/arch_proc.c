#include "log.h"
#include "cpu.h"
#include "proc.h"
#include "mm.h"
#include "cpu/gdt.h"
#include <string.h>


extern void cpu_kerneltask_trampoline(void);
extern void cpu_usertask_trampoline(void);
extern char startinit_start[], startinit_end[], startinit_lma[];


void archproc_on_schedule(proc_t* p) {
    log_msg("archproc_on_schedule\n");
    cpu_t* cpup = CURRENTCPU;
    cpup->archcpu.tss.rsp0 = p->archcpu_context.rsp0;
}

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

void archproc_prepare_startinit(proc_t* p) {
    log_msg("prepare_startinit\n");

    //Allocate new KernelStack for RSP0 (used for Exceptions and interrupts from usermode)
    page_t* rsp0stackpage = page_alloc_pages(0, 2); // 5=128kb, 4=64kb, 3=32kb, 2=16kb, 1=8kb
    adr_t rsp0stackaddr = PAGE2VIRT(rsp0stackpage);
    size_t rsp0stacksize = PAGESIZE << 2;
    log_msg("RSP0 stack addr: 0x%lx  size:0x%lx\n", rsp0stackaddr,rsp0stacksize);
    p->archcpu_context.rsp0_stackpage = (adr_t) rsp0stackaddr;
    p->archcpu_context.rsp0 = (adr_t) rsp0stackaddr+rsp0stacksize;



    size_t startinit_len = startinit_end - startinit_start;
    log_msg("Startinit SRC start addr = %#lx\n", startinit_lma);
    //log_msg("Trampoline SRC end addr = %#lx\n", trampoline_start);
    log_msg("Startinit SRC len = %d\n", startinit_len);

    size_t srcpages = (startinit_len+PAGESIZE)>>PAGESHIFT;
    log_msg("Startinit SRC pages = %d\n", srcpages);
    size_t pageorder = size_to_pageorder(startinit_len);
    log_msg("Startinit SRC pageorder = %d\n", pageorder);

    //allocate userbinary
    page_t* ubin = page_alloc_pages(0, pageorder);
    void* ubinptr = (void*) PAGE2VIRT(ubin);
    log_msg("Allocated Userbinary at %#lx\n",ubinptr);
    mmu_map_range((void*)p->upgdir,V2P(ubinptr),0x400000,startinit_len,MMU_USER) ;
    mmu_debug_walkadr((void*)p->upgdir,0x400000) ;

    log_msg("startinit_start      = %#lx\n", startinit_start);
    log_msg("P2V(startinit_start) = %#lx\n", P2V(startinit_start));

    log_msg("startinit_lma      = %#lx\n", startinit_lma);
    log_msg("P2V(startinit_lma) = %#lx\n", P2V(startinit_lma));


    memcpy(ubinptr,(void*)P2V(startinit_lma),startinit_len);


    uint64_t *sp = (uint64_t *)(p->ustackaddrvirt + p->ustacksize-64);
    sp = (uint64_t *)((uint8_t *)sp - 0);          // justera till 8 mod 16

    //iretq stack
    *(--sp) = GDT64_USER_DS | 0x03;             // ss  (RPL=3)
    *(--sp) = p->ustackaddr + p->ustacksize;    // rsp
    *(--sp) = 0x202;                            // rflags  IF=1
    *(--sp) = GDT64_USER_CS | 0x03;             // cs  (RPL=3)
    *(--sp) = 0x400000;                         // rip

    //isr_exit stack
    *(--sp) = 0;                          // errcode
    *(--sp) = 0;                          // isrno
    *(--sp) = 0;                          // rax
    *(--sp) = 0;                          // rbx
    *(--sp) = 0;                          // rcx
    *(--sp) = 0;                          // rdx
    *(--sp) = 0;                          // rdi
    *(--sp) = 0;                          // rsi
    *(--sp) = 0;                          // rbp
    *(--sp) = 0;                          // r8
    *(--sp) = 0;                          // r9
    *(--sp) = 0;                          // r10
    *(--sp) = 0;                          // r11
    *(--sp) = 0;                          // r12
    *(--sp) = 0;                          // r13
    *(--sp) = 0;                          // r14
    *(--sp) = 0;                          // r15

    //cpu_swapcontext Stack
    *(--sp) = (uint64_t)cpu_usertask_trampoline;  // "returadress" som ret hoppar till
    *(--sp) = 0;                          // rbp
    *(--sp) = 0;                          // rbx
    *(--sp) = 0;                          // r12 — smugglad entry-pekare
    *(--sp) = 0;                          // r13 — smugglat argument
    *(--sp) = 0;                          // r14
    *(--sp) = 0;                          // r15
    //*(--sp) = 0x202;                      // rflags, IF=1
    *(--sp) = 0x002;                      // rflags, IF=0, Dont set Irq on in trampoline. It will be set in finish_switch. in spinunlock.

    p->ncli = 1; //need to set ncli to 1 since in will do ncli-- in spinunlock (from finish_switch)
    p->intenable = 1;

    //p->rsp = (uint64_t)sp;
    p->archcpu_context.sp =  (uint64_t) sp;
    p->archcpu_context.cr3 = (adr_t) V2P(p->upgdir);

    return;
}