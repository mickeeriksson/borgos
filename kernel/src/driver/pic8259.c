#include "error.h"
#include "cpu/io.h"
#include "config.h"

//init stolen from pdoane intr/pic.c

#define PIC1_CMD                        0x0020
#define PIC1_DATA                       0x0021
#define PIC2_CMD                        0x00A0
#define PIC2_DATA                       0x00A1

#define ICW1_ICW4                       0x01        // ICW4 command word: 0 = not needed, 1 = needed
#define ICW1_SINGLE                     0x02        // Single mode: 0 = cascade, 1 = single
#define ICW1_ADI                        0x04        // Call address interval: 0 = interval of 8, 1 = interval of 4
#define ICW1_LTIM                       0x08        // Interrupt trigger mode: 0 = edge, 1 = level
#define ICW1_INIT                       0x10        // Initialization

#define ICW4_8086                       0x01        // Microprocessor mode: 0=MCS-80/85, 1=8086/8088
#define ICW4_AUTO                       0x02        // Auto EOI: 0 = disabled, 1 = enabled
#define ICW4_BUF_SLAVE                  0x04        // Buffered mode/slave
#define ICW4_BUF_MASTER                 0x0C        // Buffered mode/master
#define ICW4_SFNM                       0x10        // Special fully nested is programmed

#ifdef CONFIG_PIC_DISABLE
// INit PIC in disabled mode
void pic8259_init_disabled(void){
    //IRQ_STARTVECTOR = 32 //defined in config.h
    // ICW1: start initialization, ICW4 needed
    io_outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    io_outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    // ICW2: interrupt vector address
    io_outb(PIC1_DATA, IRQ_STARTVECTOR);
    io_outb(PIC2_DATA, IRQ_STARTVECTOR + 8);

    // ICW3: master/slave wiring
    io_outb(PIC1_DATA, 4);
    io_outb(PIC2_DATA, 2);

    // ICW4: 8086 mode, not special fully nested, not buffered, normal EOI
    io_outb(PIC1_DATA, ICW4_8086);
    io_outb(PIC2_DATA, ICW4_8086);

    // OCW1: Disable all IRQs
    io_outb(PIC2_DATA,0xFF);
    io_outb(PIC1_DATA,0xFF);
}
#endif

// INit PIC in disabled mode
void pic8259_init_enabled(void){
    PANIC("Not implemented");
}