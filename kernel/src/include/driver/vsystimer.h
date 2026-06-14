#ifndef _DRIVER_VSYSTIMER_H_
#define _DRIVER_VSYSTIMER_H_
#include "types.h"

struct vsystimer_info;

struct vsystimer_ops {
    uint64_t (*getmillis)(void* timerdata);
    //int (*putchar)(struct fb_info *fbinfo,int c);
    //int (*scroll)(struct fb_info *fbinfo);
};

struct vsystimer_info
{
    struct vsystimer_ops *ops;
    void* private_timerdata;
};

extern void vsystimer_init(struct vsystimer_info* info);
extern uint64_t vsystimer_millis(void);

#endif
