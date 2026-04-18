#ifndef _KCONSOLE_H_
#define _KCONSOLE_H_

#include "types.h"
#include "vfb.h"

struct kconsole_ops {
    int (*putc)(int);
    int (*getc)(void);
};

extern void kconsole_init(int (*putc)(int),int (*getc)(void));
extern void kconsole_init_vfb(struct fb_info *fbinfo);
extern void kconsole_setdefault_ops(int (*putc)(int),int (*getc)(void));

extern int kconsole_putc(int c);

/* for testing only, remove when kprintf is live.... */
extern void kconsole_print(const char *s);

/* Format a string and print it to console output */
extern int kprintf (const char *format, ...);


#endif /* __KCONSOLE_H_ */
