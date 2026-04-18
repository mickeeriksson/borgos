#include "log.h"
#include "types.h"
//#include "mutex.h"
#include <stdarg.h>

//void (*log_putchar_ptr)(char);
int (*log_putchar_ptr)(int);

//void log_init(void* putchar_ptr)
void log_init(int (*putchar_ptr)(int))
{
    log_putchar_ptr = putchar_ptr;
}


int log_putc_cb (int ch, reg_t arg){
    return log_putchar_ptr(ch);
}


extern int _doprnt(const char *format, va_list ap, int (*putc_func) (int, reg_t), reg_t putc_arg);
void log_msg(const char *format, ...)
{
    va_list args;													// Argument list
    //int count;														// Number of characters printed
    va_start(args, format);											// Create argument list
    //count = _doprnt(fmt, args, prn_to_func, Console_WriteChar);
    //count = _doprnt(format, args, log_putc_cb, 0);
    _doprnt(format, args, log_putc_cb, 0);
    va_end(args);													// Done with argument list
    //return count;													// Return number of characters printed
}

void log_vmsg(const char *format, va_list vaList)
{
    _doprnt(format, vaList, log_putc_cb, 0);
}


