#ifndef _IRQ_H_
#define _IRQ_H_

//typedef void (*IrqHandler)(void);
typedef enum { IRQ_NOT_KNOWN, IRQ_NOT_MINE, IRQ_HANDLED } irq_status_t;
typedef irq_status_t (*irq_handler_fn)(void *ctx);

extern void irq_inithandlers(void);
//extern void irq_add_irq_handler(uint8_t irq,IrqHandler irqhandler);
extern void irq_add_irq_handler(uint8_t isrno,irq_handler_fn fn,void* ctx);
//extern void irq_cpu_localtimer_tick_cb(void);
extern irq_status_t irq_cpu_localtimer_tick_cb(void *ctx);
//extern void irq_process_irq(uint8_t irq);
extern void irq_process_irq(uint8_t isrno,uint8_t irqno);

extern uint8_t irq_get_free_msi(void);
extern adr_t isr_get_isr_stub_addr(uint8_t isrno);

typedef struct irq_handler {
    irq_handler_fn      fn;
    void               *ctx;      //device data
    struct irq_handler *next;
} irq_handler_t;

#endif
