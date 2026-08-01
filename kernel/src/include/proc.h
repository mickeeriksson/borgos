#ifndef _PROC_H
#define _PROC_H

#include "types.h"
#include "list.h"
#include "cpu/cpu_cpucontext.h"



enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE, KERNELIDLEPROC };
extern char* procstatename[7]; // = {"UNUSED","EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE","KERNELIDLEPROC"};

typedef struct proc {
    /*
    struct proc *next;    //no need for this, but keep it for the next for entries to keep there position for now
    uint32_t    *ksp;     // at 4
    uint32_t    *usp;     // at 8 : Umode sp at time of syscall
    //uint32_t    *upc;     // at 12: linkR at time of syscall,, //redundant remove????
    //uint32_t    *spsr;    // at 16: Umode cpsr
*/
    struct list_head plist;              //list of procs used for lists (SLEEPING, RUNNABLE, RUNNING) used in scheduler.c

    uint32_t pid;                        // Process ID
    enum procstate state;           // Process state
    char name[33];               // Process name (debugging)

    //memory stuff
#ifndef NOMMU
    adr_t pgdir;                // Page table, OBS virtual address,
#endif
    //struct trapframe *tf;        // Trap frame for current syscall
    adr_t kstackaddr;                // Bottom of kernel stack for this process  (as kernel VA addr)
    size_t kstacksize;

    uint64_t slice_ticks;           //ticks left in this slice (before preemt)

    int32_t ncli;                    // Depth of pushcli nesting.
    int intenable;              // Were interrupts enabled before pushcli?, 
    arch_cpu_context_t archcpu_context;

    //adr_t context;               // stackpointer of context, when doing swap in scehdule.

    //adr_t ustack_start;        // Bottom of user stack for this process (as user VA addr)
    //adr_t ustack_top;          // Top of user stack for this process, (normaly KERNEL_VIRTUAL_BASE) (as user VA addr)


    //filehandling
    //struct file *fs_openfiles[VFS_MAXP_OFILE];  // Open files
    //struct inode *fs_cwd;           // Current directory
    //struct inode *fs_root;           // Root fs

    //reg_t sleepmillis;



    /*
    uint32_t* ustack_start;        // Bottom of user stack for this process (as user VA addr)
    uint32_t* ustack_top;          // Top of user stack for this process, (normaly KERNEL_VIRTUAL_BASE) (as user VA addr)
    uint32_t pmemsize;             // Size of process memory (bytes), split to code,heap(brk) later on.

    int cpuId;                      // running on CPU ID;
    //volatile int pid;            // Process ID

    int insvchandler;               // set to >0 of this proc is currently handling a svc syscall
    uint64_t sleepuntil;            //if 0 sleep until specific condition/irq are met, if >0 sleep until system millis > sleepuntil.
    //struct context *context;     // Switch here to run process
    //int killed;                  // If non-zero, have been killed
    //char name[16];               // Process name (debugging)
*/
} proc_t;



extern void proc_init(void);
extern proc_t* proc_alloc(void);
extern uint32_t proc_newpid(proc_t* p);
extern proc_t* proc_create_kernelproc(char* name);

extern void archproc_prepare_kernelproc_stack(proc_t* p,void (*entry)(void *), void *arg) ;

#endif