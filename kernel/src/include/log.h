#ifndef _LOG_H_
#define _LOG_H_
#include <stdarg.h>

//extern void (*log_putchar_ptr)(char);
extern int (*log_putchar_ptr)(int);

//void log_init(void* putchar_ptr);
void log_init(int (*putchar_ptr)(int));
void log_msg(const char *format, ...);
void log_vmsg(const char *format, va_list vaList);

#endif