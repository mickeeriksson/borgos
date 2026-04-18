#ifndef __CPU_x86_64_SPINLOCK_H_
#define __CPU_x86_64_SPINLOCK_H_

#include "cpu/cpu_types.h"
#include "cpu.h"
#include "log.h"
//#include "error.h"

#define SPIN_LOCKED 1
#define SPIN_UNLOCKED 0


static inline uint8_t _cpu_trylock(volatile uint8_t *lock, uint8_t newval)
{
    uint8_t result;
    //log_msg("_cpu_trylock before=%d newval=%d\n",*lock,newval);

// The + in "+m" denotes a read− modify− write operand.
__asm__ volatile("lock; xchgb %0, %1"
    : "+m" (*lock), "=a" (result)
    : "1" (newval)
    : "cc");
    //log_msg("_cpu_trylock res=%d with newval=%d\n",result,newval);
    return result;
}

static inline void cpu_spinlock(volatile void* lock){
    uint8_t ret;
    ret = _cpu_trylock(lock,SPIN_LOCKED);
    if(ret == SPIN_UNLOCKED){
        //log_msg("cpu_spinlock:lock is = %d, success at first attemt\n",ret);
        //old value was UNLOCKED == success
        return;
    }

    //        irq_restore();
    do{
        volatile uint8_t tlock = *(uint8_t *)lock;
        if(tlock == SPIN_LOCKED){
            //log_msg("cpu_spinlock:current value of lock is = %d, pause a little\n",tlock);
            //mdelay(1000);
            __asm__ volatile("pause");
            continue;
        }
        //        irq_save();
        ret = _cpu_trylock(lock,SPIN_LOCKED);
        if(ret == SPIN_UNLOCKED){
            //old value was UNLOCKED == success
            //log_msg("cpu_spinlock:lock ret is = %d, success at nnn attemt\n",ret);
            return;
        }else{
            //log_msg("cpu_spinlock:lock ret is = %d, success at nnn attemt\n",ret);
            //damn it, someone got before us....
            //        irq_restore();
        }
    }while(1);
}
static inline void cpu_spinunlock(void* lock){
    uint8_t ret;
    volatile uint8_t tlock = *(uint8_t *)lock;
    if(tlock == SPIN_UNLOCKED){
        log_msg("cpu_spinunlock:LOCK IS NOT LOCKED!");
        while(1){
            log_msg(".");
        }
    }
    ret = _cpu_trylock(lock,SPIN_UNLOCKED);
    if(ret==SPIN_UNLOCKED){
        log_msg("unlock an already locked lock at %#x \n",lock);
    }
    //log_msg("cpu_spinunlock:lock ret is = %d, Lock is now unlocked\n",ret);
}



typedef struct arch_spinlock{
    uint8_t lock;
}spinlock_t;

#define SPINLOCK(x)  spinlock_t x = {.lock=SPIN_UNLOCKED }

static inline void spinlock(spinlock_t *lck){
    irq_save();
#ifdef SMP
    cpu_spinlock(&lck->lock);
#endif
}
static inline void spinunlock(spinlock_t *lck){
#ifdef SMP
    cpu_spinunlock(&lck->lock);
#endif
    irq_restore();
}

#endif
