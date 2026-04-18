
#include "types.h"
//#include "kconsole.h"
#include "vfb.h"
#include <stdarg.h>

#include "error.h"
#include "spinlock.h"

extern int _doprnt(const char *format, va_list ap, int (*putc_func) (int, reg_t), reg_t putc_arg);


struct kconsole_ops kconsole_default_ops={
        .putc=0,
        .getc=0,
};
struct kconsole_ops *kconsole_ops=&kconsole_default_ops;

struct fb_info *kconsole_fbinfo = NULL;

SPINLOCK(kconsolespin);

void kconsole_init(int (*putc)(int),int (*getc)(void))
{
    kconsole_ops = &kconsole_default_ops;
    kconsole_ops->putc = putc;
    kconsole_ops->getc = getc;
}

void kconsole_init_vfb(struct fb_info *fbinfo) {
    kconsole_fbinfo = fbinfo;
}


void kconsole_setdefault_ops(int (*putc)(int),int (*getc)(void)){
    kconsole_ops->putc = putc;
    kconsole_ops->getc = getc;
}

int kconsole_putc(int c){
    //assert(kconsole_default_ops.putc>0));
    if(kconsole_fbinfo!=NULL && kconsole_fbinfo->fbops->putchar!=NULL){
        kconsole_fbinfo->fbops->putchar(kconsole_fbinfo,c);
        return 0;
    }

    if(kconsole_ops->putc==NULL)
        return EDEV_NOTREADY;
    kconsole_ops->putc(c);
    return 0;
}

int kconsole_getc(void){
    //assert(kconsole_default_ops.getc>0));
    return EDEV_NOTREADY;
}

/* for testing only, remove when kprintf is live.... */
//void kconsole_print(const char *s) {
//    while(*s != '\0') {
//        kconsole_putc(*s);
//        s++;
//    }
//}


int kprintf_putc_cb (int ch, reg_t arg){
    return kconsole_putc(ch);
}

/* Format a string and print it to console output */
int kprintf (const char *format, ...)
{
    spinlock(&kconsolespin);

    va_list args;													// Argument list
    int count;														// Number of characters printed
    va_start(args, format);											// Create argument list
    //count = _doprnt(fmt, args, prn_to_func, Console_WriteChar);
    count = _doprnt(format, args, kprintf_putc_cb, 0);
    va_end(args);													// Done with argument list

    spinunlock(&kconsolespin);
    return count;													// Return number of characters printed
}