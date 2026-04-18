#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include "cpu/cpu_spinlock.h"

/* must be defined in in cpu_inc.S */
//extern int spinlock(void* lock);
//extern int spinunlock(void* lock);

extern void spinlock(spinlock_t *lock);
extern void spinunlock(spinlock_t *lock);


#endif