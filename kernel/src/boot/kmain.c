#include "types.h"
#include "log.h"
#include "kconsole.h"
#include "mm.h"
//extern void hal_init_early(void);


adr_t bootimage_start = 0; //This should be a phys address
adr_t bootimage_end = 0; //This should be a phys address

extern void hal_bp_init(void);

/*
    When kmain is called.
        * log_init has been called to set *log_putchar_ptr function pointer
        *
 */

void kmain_bp_enter(void){
    log_msg("kmain: BP Enter!\n");
    kprintf("kmain: BP Enter!\n");
    //hal_init_early();

    hal_bp_init();
    kprintf("BOOT:Init HAL (for BSP) [OK]\n");

    log_msg("***********************************************************************\n");
    log_msg("*                    START DRIVER INIT!                               *\n");
    log_msg("***********************************************************************\n");

    /*
    void* a = kmalloc(16,NULL);
    void* b = kmalloc(16,NULL);
    void* c = kmalloc(16,NULL);
    log_msg("Malloc a=0x%lx b=0x%lx c=0x%lx\n",a,b,c);
    kmalloc_debug_walk();
    kfree(a);
    kfree(b);
    kfree(c);
    kmalloc_debug_walk();
    */

    int i=0;
    while(1){
        i+=1;
        //HANG HERE
    }
}


/*
void kmain(void){

    //hal_init_early();

    int i=0;
    while(1){
        i+=1;
        //HANG HERE
    }
}*/