#include "proc.h"
#include "log.h"
#include "spinlock.h"
#include "error.h"
#include "mm.h"
#include <string.h>
#include "cpu/cpu_cpu.h"

char* procstatename[] = {"UNUSED","EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE","KERNELIDLEPROC"};

//proc_t* proc_setupidleproc(proc_t* p,cpu_t *ccpu);

SPINLOCK(proctablespin);

struct proctable{
    //MUTEX lock;
    proc_t* proc[MAXPROC];
} ;

struct proctable ptable;


void proc_init(void)
{
    log_msg("proc_init\n");
    //mutex_init(&ptable.lock);
    //mutex_lock(&ptable.lock);
    spinlock(&proctablespin);
    for(int i=0; i<MAXPROC;i++){
        ptable.proc[i]=0;
    }
    //proc_t* idleproc = proc_allocidleproc(get_cpu_id());
    //ptable.proc[0]=idleproc;
    //proc_setupidleproc(idleproc,CURRENTCPU);
    //mutex_unlock(&ptable.lock);
    //mutex_lock(&ptable.lock);
    spinunlock(&proctablespin);
}

proc_t* proc_alloc(void) {
    proc_t* p;

    if(  (p = (proc_t*) kmalloc(sizeof(proc_t),0)) ==0 ){
        PANIC("No more mem for a new proc");
    }

    memset(p, 0, sizeof(proc_t));
    p->state = EMBRYO;
    p->pid = 0;
    p->ncli=0;
    INIT_LIST_HEAD(&(p->plist));
#ifndef NOMMU
    p->pgdir=0;                // Page table, OBS virtual address,
#endif

    return p;
}

proc_t* proc_create_kernelproc(char* name) {
    proc_t* p = proc_alloc();
    uint32_t pid = proc_newpid(p);

    //Allocate new stack for BP
    page_t* stackpage = page_alloc_pages(0, 2); //64Kb  5=128, 4=64, 3=32, 2=16, 1=8
    adr_t stackaddr = PAGE2VIRT(stackpage);
    size_t stacksize = PAGESIZE << 2;
    log_msg("BP stack addr: 0x%lx  size:0x%lx\n", stackaddr,stacksize);


    //adr_t newrsp = stackaddr + stacksize - 64;
    //log_msg("Set BP stackpointer to : 0x%lx\n", newrsp);

    //setup proc_t for bp
    strlcpy(&p->name[0], name, 32);
    p->kstackaddr = stackaddr;
    p->kstacksize = stacksize;
    p->pgdir = (adr_t) kernelpagetable;
    p->slice_ticks = 0x0FFFFFFFFFFFFFFF;
    p->state=EMBRYO;  //dont preemp for now.


    log_msg("Created Kernelproc %d\n",pid);
    return p;
}


uint32_t proc_newpid(proc_t* p) {
    log_msg("proc_newpid\n");

    uint32_t pid=0;
    spinlock(&proctablespin);
    for(int i=1; i<MAXPROC;i++){
        if (ptable.proc[i]==0) {
            ptable.proc[i]=p;
            pid = i;
            break;
        }
    }
    spinunlock(&proctablespin);

    if (pid>0) {
        p->pid = pid;
        return pid;
    }else {
        PANIC("No avaliable pid\n");
    }
    return 0;
}

/*
proc_t* currentproc(void){
    pushcli();
    if(cpu_read_irq()){
        PANIC("pushcli - didnt stick");
    }
    volatile int cpuId=get_cpu_id();
    proc_t *p = cpu[cpuId].currentproc;
    if(cpu_read_irq()){
        PANIC("popcli - before call error");
    }
    popcli();
    return p;
  //return 0;
}
*/

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and return it.
// Otherwise return 0.
/*
proc_t* allocproc(void)
{
    proc_t* p;

    mutex_lock(&ptable.lock);
    for(int i=0; i<MAXPROC;i++){
        //ptable.proc[i]=0;
        p=ptable.proc[i];
        if(p==0){
            log_msg("Found empty proc for pid %d \n",i);
            if(  (p = (proc_t*) kmalloc(sizeof(proc_t),0,"proc_t")) ==0){
                PANIC("No more mem for a new proc");
            }
            //cant assume that kmalloc returned zeroed block;
            ptable.proc[i]=p;
            memset(p, 0, sizeof(proc_t));
            p->state = EMBRYO;
            p->pid = i;
            goto found;
        }else if(p->state == UNUSED){
            PANIC("TODO, figure out if zero UNUSED PROC or assume all cleared???");
            //memset(p, 0, sizeof(proc_t));

            p->state = EMBRYO;
            p->pid = i;
            goto found;
        }
    }
    mutex_unlock(&ptable.lock);
    return 0;

found:
    mutex_unlock(&ptable.lock);

    log_msg("Allocate proc %d  at addr=0x%x\n",p->pid,p);

    //clear fd array;
    for(int fd=0; fd<VFS_MAXP_OFILE; fd++){
        p->fs_openfiles[fd]=NULL;
    }

    // Allocate kernel stack if necessary.
    if(KERNEL_STACK_SIZE != PAGESIZE){
        PANIC("KERNEL_STACK_SIZE != PAGESIZE");
    }
    //void* kstack =
    p->kstack = (adr_t) alloc_page(0,"KSTACK");
    memset((void *) p->kstack, 0, KERNEL_STACK_SIZE);
    //adr_t newproc_stackptr = (adr_t) (((adr_t)kstack) + KERNEL_STACK_SIZE);
    log_msg("Kernelstack for proc %d start=0x%x\n",p->pid,p->kstack);
//    //log_msg("Kernelstack for proc %d start=0x%x end 0x%x\n",p->pid,p->kernel_stack,newproc_stackptr);
//    int ssize = (KERNEL_STACK_SIZE/4); //nr of 32bit entries in a KSTACK
//    p->ksp = &(p->kstack[ssize-1]);


    p->pgdir = vm_setup_uvm();

    p->state = EMBRYO;
//    p->insvchandler=0;
    p->fs_cwd=rootinode;
    p->fs_root=rootinode;

    kprintf("proc:allocproc %d at 0x%x\n",p->pid, p);
    return p;
}
*/
