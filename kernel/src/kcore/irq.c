#include "cpu.h"
#include "error.h"
#include "log.h"
#include "irq.h"
#include "cpu/cpu_getcpu.h"

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli.  Also, if interrupts
// are off, then pushcli, popcli leaves them off.


IrqHandler irqhandler[256];

//TODO, reqwrite to make each irq a vector of handlers to allow an irq to trigger multiple handlers

void irq_inithandlers(void) {
    for (int i = 0; i < 256; i++) {
        irqhandler[i] = NULL;
    }
}

void irq_add_irq_handler(uint8_t irq,IrqHandler handler) {
    irq_save();
    irqhandler[irq]=handler;
    irq_restore();
}

void irq_process_irq(uint8_t irq){
    if (irqhandler[irq] != NULL) {
        irqhandler[irq]();
    }
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
}

void irq_cpu_localtimer_tick_cb(){
    cpu_t* cpu = CURRENTCPU;
    int irqOn = cpu_read_irq();
    log_msg("intenable=%d irqon=%d\n",cpu->intenable,irqOn);
    log_msg("TICK CPU[%d]\n",cpu->cpuid);
    log_msg("intenable=%d irqon=%d\n",cpu->intenable,cpu_read_irq());
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

    if(current_cpu->ncli == 0){
        current_cpu->intenable = irqOn;
    }
    current_cpu->ncli++;

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
    if(--current_cpu->ncli < 0)
        PANIC("popcli called 1 more time then pushcli?");  //called popcli 1 more time then pushcli??
    if(current_cpu->ncli == 0 && current_cpu->intenable){
        //log_msg("spinlock:popcli CALL sti(), ncli=%d, intenable=%d cpu=0x%x\n",current_cpu->ncli,current_cpu->intenable,current_cpu);
        cpu_enable_irq();
    }else{
        //if(current_proc)
        //    log_msg("spinlock:popcli NO call sti(), ncli=%d, intenable=%d cpu=0x%x, pid=%d\n",current_cpu->ncli,current_cpu->intenable,current_cpu,current_proc->pid);
        //else
        //    log_msg("spinlock:popcli NO call sti(), ncli=%d, intenable=%d cpu=0x%x, pid=KERNEL\n",current_cpu->ncli,current_cpu->intenable,current_cpu);
    }

}
