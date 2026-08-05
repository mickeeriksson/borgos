#include "cpu.h"
#include "error.h"
#include "log.h"
#include "irq.h"
#include "scheduler.h"
#include "spinlock.h"
#include "mm.h"
#include "cpu/cpu_getcpu.h"

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.


//IrqHandler irqhandler[MAX_INT_VECTORS];

static irq_handler_t *isr_chain[MAX_INT_VECTORS];           //index in isr-vectors (NOT IRQ Vecotrs???)
static spinlock_t     isr_chain_lock[MAX_INT_VECTORS];

//TODO, reqwrite to make each irq a vector of handlers to allow an irq to trigger multiple handlers

void irq_inithandlers(void) {
    for (int i = 0; i < MAX_INT_VECTORS; i++) {
        //irqhandler[i] = NULL;
        isr_chain[i] = NULL;
    }

    for (int i = 0; i < MAX_INT_VECTORS; i++) {
        spinlock_init(&isr_chain_lock[i]);
    }

}

//void irq_add_irq_handler(uint8_t irq,IrqHandler handler) {
void irq_add_irq_handler(uint8_t isrno,irq_handler_fn fn,void* ctx) {
    //if (irqno >= MAX_INT_VECTORS) {
    //    PANIC("irq_add_irq_handler: irqno >= MAX_INT_VECTORS");
    //}

    irq_handler_t *h = kmalloc(sizeof(irq_handler_t),0);
    h->fn = fn;
    h->ctx = ctx;

    spinlock(&isr_chain_lock[isrno]);
    h->next = isr_chain[isrno];
    isr_chain[isrno] = h;
    spinunlock(&isr_chain_lock[isrno]);
}

uint8_t irq_get_free_msi(void) {
    for (int i = MSIIRQ_STARTVECTOR; i < MSIIRQ_STARTVECTOR+MSIIRQ_COUNT; i++) {
        if (isr_chain[i] == NULL) {
            return i;
        }
    }
    return 0;
}


void irq_process_irq(uint8_t isrno,uint8_t irqno){
    //if (irqhandler[irq] != NULL) {
    //    irqhandler[irq]();
   // }
    /*
    let elistlock = &mut IRQHANDLERS[irq as usize].lock();
    let elist : &mut Vec<IrqHandlerInfo> = elistlock.as_mut();

    if elist.len() == 0 {
        warn!("No registered handlers for IRQ {}",irq);
    }

    for e in &mut *elist {
        debug!("Delegate to IRQ HANDLER ENTRY {:?}",e);
        (e.irq_cb)(e.dev);
    }*/


    irq_handler_t *h = isr_chain[isrno];
    uint8_t handled = 0;

    while (h) {
        if (h->fn(h->ctx) == IRQ_HANDLED) {
            handled = 1;
        }
        h = h->next;
    }

    if (!handled) {
        // spurious/delad IRQ som ingen kändes vid – logga, räkna, ev. maskera
    }

    //lapic_send_eoi();  // EOI skickas EN gång, efter hela kedjan
    // eoi görs i hal isr.c
}

//void irq_cpu_localtimer_tick_cb(){
irq_status_t irq_cpu_localtimer_tick_cb(void *ctx){
    cpu_t* cpu = CURRENTCPU;
    //int irqOn = cpu_read_irq();
    cpu->ticks++;
    //log_msg("intenable=%d irqon=%d\n",cpu->intenable,irqOn);

    //log_msg("intenable=%d irqon=%d\n",cpu->intenable,cpu_read_irq());
    proc_t *p = cpu->currentproc;
    if (p) {
        if (p->slice_ticks>0) {
            p->slice_ticks--;   //count down ticks to preemtion.
        }
        //log_msg("TICK CPU[%d]:%d slice:%#lx\n",cpu->cpuid,cpu->ticks,p->slice_ticks);
    }
    // check sleeping tasks

    if (p->ncli == 0) {
        //no cpu_save_calls.
        //no locks hold (since ncli is also ++, at call irq_save() in  spinlock
        //do this better in future by check number of locks held by proc_t

        //try schedule this process.
        scheduler_schedule();
    }else {
        log_msg("Skip scheduler_schedule cpu->ncli=%dh\n",p->ncli);
    }
    return IRQ_HANDLED;
}


// turn off irq
void irq_save(void)
{
    cpu_t *current_cpu = CURRENTCPU;
    //volatile int irqOn = read_irq();
    //log_msg("spinlock:pushcli (before cli) ncli=%d, intenable=%d cpu=0x%x eflags=0x%x,  currentproc=0x%x \n",current_cpu->ncli,current_cpu->intenable,current_cpu,eflags,current_proc);
    //log_msg("spinlock:pushcli (before cli) ncli=%d, intenable=%d cpu=0x%x  \n",current_cpu->ncli,current_cpu->intenable,current_cpu);

    volatile int irqOn = cpu_disable_irq();
    volatile int irqOn2 = cpu_read_irq();
    if(irqOn2){
        PANIC("pushcli - didnt stick");
    }

    //log_msg("irq_save cpuid=%d at %#x ncli (before)=%d\n",current_cpu->cpuid, current_cpu,current_cpu->ncli);

    if(current_cpu->currentproc->ncli == 0){
        current_cpu->currentproc->intenable = irqOn;
    }
    current_cpu->currentproc->ncli++;

    volatile int irqOn3 = cpu_read_irq();
    if(irqOn3){
        PANIC("pushcli - changed");
    }

    //if(current_proc)
    //    log_msg("spinlock:pushcli (after cli) ncli=%d, intenable=%d cpu=0x%x, pid=%d\n",current_cpu->ncli,current_cpu->intenable,current_cpu,current_proc->pid);
    //else
    //    log_msg("spinlock:pushcli (after cli) ncli=%d, intenable=%d cpu=0x%x, pid=KERNEL\n",current_cpu->ncli,current_cpu->intenable,current_cpu);

}

//restore irq
void irq_restore(void)
{
    int irqOn = cpu_read_irq();
    cpu_t *current_cpu = CURRENTCPU;
    //log_msg("irq_restore cpuid=%d at %#x ncli (before)=%d\n",current_cpu->cpuid, current_cpu,current_cpu->ncli);
    if(irqOn)
        PANIC("popcli - interruptible");
    if(--current_cpu->currentproc->ncli < 0)
        PANIC("popcli called 1 more time then pushcli?");  //called popcli 1 more time then pushcli??
    if(current_cpu->currentproc->ncli == 0 && current_cpu->currentproc->intenable){
        //log_msg("spinlock:popcli CALL sti(), ncli=%d, intenable=%d cpu=0x%x\n",current_cpu->ncli,current_cpu->intenable,current_cpu);
        cpu_enable_irq();
    }else{
        //if(current_proc)
        //    log_msg("spinlock:popcli NO call sti(), ncli=%d, intenable=%d cpu=0x%x, pid=%d\n",current_cpu->ncli,current_cpu->intenable,current_cpu,current_proc->pid);
        //else
        //    log_msg("spinlock:popcli NO call sti(), ncli=%d, intenable=%d cpu=0x%x, pid=KERNEL\n",current_cpu->ncli,current_cpu->intenable,current_cpu);
    }

}
