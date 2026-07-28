#include "types.h"
#include <stdarg.h>
#include "string.h"


extern int _doprnt(const char *format, va_list ap, int (*putc_func) (int, reg_t), reg_t putc_arg);


/*
 * Routine called by _doprnt() to output each character.
 */
static int sprntf_cb(int c, reg_t _sptr)
{
    char **sptr = (char **)_sptr;
    char *s = *sptr;

    *s++ = c;
    *sptr = s;
    return (unsigned char)c;
}


int sprintf(char *str, const char *format, ...)
{
    va_list ap;
    char *s;

    s = str;
    va_start(ap, format);
    _doprnt(format, ap, sprntf_cb, (reg_t)&s);
    va_end(ap);
    *s = '\0';

    return s - str;
}