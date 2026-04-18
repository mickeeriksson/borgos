#include "types.h"
//#include "defs.h"
#include "cpu/io.h"
#include <stdarg.h>

#define PORT 0x3f8   /* COM1 */

/*
void bootdebug_init(void) __attribute__ ((section (".text.boot")));
static uint8_t bootdebug_is_transmit_empty(void) __attribute__ ((section (".text.boot")));
void bootdebug_putc(uint8_t c)  __attribute__ ((section (".text.boot")));
static void bootdebug_itoa (char *buf, int base, long d) __attribute__ ((section (".text.boot")));
void bootdebug_printf (const char *format, ...) __attribute__ ((section (".text.boot")));
*/

void bootdebug_init() {
    io_outb(PORT + 1, 0x00);    // Disable all interrupts
    io_outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    io_outb(PORT + 0, 0x01);    // Set divisor to 3 (lo byte) 115200 baud
    io_outb(PORT + 1, 0x00);    //                  (hi byte)
    io_outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    io_outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    //outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    io_outb(PORT + 4, 0x00);    // IRQs disabled,

    io_outb(PORT,'A');
    io_outb(PORT,'B');
}

static uint8_t bootdebug_is_transmit_empty(void) {
    uint8_t val = io_inb(PORT + 5) & 0x20;
    return val;
}

void bootdebug_putc(uint8_t c) {
    while (bootdebug_is_transmit_empty() == 0);
    io_outb(PORT,c);
}

static void bootdebug_itoa (char *buf, int base, long d)
{
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;

    /* If %d is specified and D is minus, put `-' in the head. */
    if (base == 'd' && d < 0)
    {
        *p++ = '-';
        buf++;
        ud = -d;
    }
    else if (base == 'x')
        divisor = 16;

    /* Divide UD by DIVISOR until UD == 0. */
    do
    {
        int remainder = ud % divisor;

        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
    while (ud /= divisor);

    /* Terminate BUF. */
    *p = 0;

    /* Reverse BUF. */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2)
    {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

/* Format a string and print it to serial */
void bootdebug_printf (const char *format, ...) {
    char **arg = (char **) &format;
    int c;
    char buf[200];
    int32_t integer = 0;
    va_list args;
    va_start(args, format);

    arg++;
    while ((c = *format++) != 0) {
        if (c != '%')
            bootdebug_putc(c);
        else {
            char *p, *p2;
            int pad0 = 0, pad = 0;

            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }

            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }

            switch (c) {
                case 'c':
                    p = va_arg(args, char*);
                    bootdebug_putc(*p);
                    break;

                case 'd':
                case 'u':
                case 'x':
                    integer = va_arg(args, int32_t);
                    bootdebug_itoa(buf, c, integer);
                    p = buf;
                    goto string;
                    break;

                case 's':
                    //p = *arg++;
                    p = va_arg(args, char*);
                    if (!p)
                        p = "(null)";

                string:
                    for (p2 = p; *p2; p2++);
                    for (; p2 < p + pad; p2++)
                        bootdebug_putc(pad0 ? '0' : ' ');
                    while (*p)
                        bootdebug_putc(*p++);
                    break;

                default:
                    bootdebug_putc(*((int *) arg++));
                    break;
            }
        }
    }
}