#ifndef _IRQ_H_
#define _IRQ_H_

typedef void (*IrqHandler)(void);

extern void irq_inithandlers(void);
extern void irq_add_irq_handler(uint8_t irq,IrqHandler irqhandler);
extern void irq_cpu_localtimer_tick_cb(void);
extern void irq_process_irq(uint8_t irq);

#endif
