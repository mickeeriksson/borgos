#include "proc.h"
#include "log.h"
#include "spinlock.h"
#include "error.h"
#include "mm.h"
#include <string.h>

//#define SCHLOG(...)    log_msg(__VA_ARGS__)
#define SCHLOG(...)

struct list_head sleepinglist;
struct list_head runnablelist;
struct list_head runninglist;

SPINLOCK(schedulerqueuespin);
SPINLOCK(schedulerspin);

int schedulercounter = 0;

void scheduler_init(void) {
    //set up structures. Need to be done before enabling interrupts. (below), but no process in any queues yet!


    INIT_LIST_HEAD(&sleepinglist);
    INIT_LIST_HEAD(&runnablelist);
    INIT_LIST_HEAD(&runninglist);

    log_msg("Scheduler Init [OK]\n");

}

void __scheduler_enqueue(proc_t* p)
{
    p->state=RUNNABLE;
    list_del(&(p->plist));
    list_add_tail(&(p->plist), &(runnablelist));
}

void scheduler_enqueue(proc_t* p)
{
    spinlock(&schedulerqueuespin);
    __scheduler_enqueue(p);
    spinunlock(&schedulerqueuespin);
}

proc_t* scheduler_dequeue(){
    proc_t* p=0;

    spinlock(&schedulerqueuespin);
    int isempty = list_empty(&(runnablelist));
    if(isempty){
        spinunlock(&schedulerqueuespin);
        return 0;
    }
    p = list_entry(runnablelist.next, proc_t, plist);
    list_del(&(p->plist));
    //kprintf("About to dequeue pid=%d, state=%s %s\n", p->pid, procstatename[p->state]);
    spinunlock(&schedulerqueuespin);
    return p;
}


void scheduler_markkernelidle(proc_t* p){
    spinlock(&schedulerqueuespin);
    p->state=KERNELIDLEPROC;
    //remove page from whatever list it belongs to!
    list_del(&(p->plist));
    spinunlock(&schedulerqueuespin);
}

void scheduler_markrunning(proc_t* p){
    spinlock(&schedulerqueuespin);
    p->state=RUNNING;
    //remove page from whatever list it belongs to!
    list_del(&(p->plist));
    //add to inuselist
    list_add_tail(&(p->plist), &(runninglist));
    spinunlock(&schedulerqueuespin);
}

extern void cpu_swapcontext(arch_cpu_context_t* prev, arch_cpu_context_t* new);

void scheduler_switch_process(proc_t *prev, proc_t *newproc)
{
    newproc->slice_ticks=5;
    //switch_vm(new->page_map);
    //pgd_t* oldttb = pgtable_switch_userttb(new->pgdir);
    //mmu_tlb_flush_all();
    log_msg("scheduler_switch_process TODO, fix TLB flush\n");
    cpu_swapcontext(&prev->archcpu_context, &newproc->archcpu_context);
}

void scheduler_task_exit(void) {
    //what to do here??
    //mark thread as zombie.
    //cleanup???
    PANIC("scheduler_task_exit");
}

void finish_switch(void) {
    //same as in scheduler_schedule.
    schedulercounter--;
    spinunlock(&schedulerspin);
}

void scheduler_start_kerneltask(void (*entry)(void *), void *arg) {
    finish_switch();               /* samma delade unlock-logik som resume-vägen använder */
    entry(arg);                    /* kör tradens faktiska arbete — förväntas aldrig returnera */
    scheduler_task_exit();         /* försvarslinje ifall entry ändå gör det */
    __builtin_unreachable();
}


void scheduler_schedule(void) {
    proc_t *p;
    cpu_t* cpu;
    proc_t *current_proc;
    SCHLOG("Scheduler Entry, schedulerspin=%d\n",schedulerspin);
    spinlock(&schedulerspin);
    schedulercounter++;

    if(schedulercounter!=1){
        log_msg("Schedulercounter>1 Should never happpen?? Is spinlock broken?\n");
        log_msg("  This is because of cpu_kerneltask_trampoline\n");
        log_msg("  must have stub in scheduler that relases lock, and fix interrupts\n");
        log_msg("  maybe IF flag should be of until this is fixed?\n");
        PANIC("Schedulercounter>1 Should never happpen?? Is spinlock broken?\n");
    }

    cpu = CURRENTCPU;
    current_proc = cpu->currentproc;
    int doSchedule=0;

    //Check if any higher processes exist.....

    //check if current proc_t timeslice is up.
    if (current_proc->slice_ticks==0) {
        SCHLOG("  slice_ticks==0, do schedule\n");
        doSchedule=1;
    }

    if (doSchedule==1) {
        SCHLOG("  doSchedule!\n");
        p=scheduler_dequeue();

        if(p==0){
            SCHLOG("Runnable queue empty current_proc=%d state=%d[%s]\n",current_proc->pid,current_proc->state,procstatename[current_proc->state]);
            if(current_proc->state==RUNNING){
                SCHLOG("scheduler:schedule No BETTER PROCS, Keep running \n");
                schedulercounter--;
                spinunlock(&schedulerspin);
                SCHLOG("Scheduler Exit (early),  schedulerspin=%d\n",schedulerspin);
                return;
            }else{
                p=cpu->idleproc;
                SCHLOG("scheduler:schedule No WORK TO BE DONE JUST IDLE! currentpid=%d, newpid=%d, cpu=%d\n",current_proc->pid,p->pid,cpu->cpuid);
            }
        }
        if (p) {
            // Switch to chosen process.
            SCHLOG("Switch to proc %d\n",p->pid);


            //Enque to old proc
            if(current_proc==cpu->idleproc){
                //dont enque the idle proc.
                //just mark as KERNEL0
                scheduler_markkernelidle(current_proc);
            }else{
                if(current_proc->state==RUNNING){
                    scheduler_enqueue(current_proc);
                }
            }


            scheduler_markrunning(p);
            //log_msg("Proc table before switchproc (as status will be)\n");
            //scheduler_debug_ps();
            SCHLOG("scheduler:schedule cpu=%d switch from pid %d to pid %d\n",cpu->cpuid,current_proc->pid,p->pid);
            //do the switch to current_proc
            cpu->currentproc = p;
            scheduler_switch_process(current_proc, p);
            log_msg("AFTER SWTICH\n");
        }else{
            PANIC("Nothing to switch to!\n");
        }
    }

    //next 2 lines are the same as in finish_switch
    schedulercounter--;
    spinunlock(&schedulerspin);
    SCHLOG("Scheduler Exit,  schedulerspin=%d\n",schedulerspin);
}