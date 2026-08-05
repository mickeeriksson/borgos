#include "types.h"
#include "log.h"
#include "kconsole.h"
#include "mm.h"
#include "cpu/io.h"
#include "hwdb.h"
#include "driver/pci/pci.h"
#include "driver/driver.h"
#include "driver/usb/usb.h"
#include "delay.h"

#include "cpu.h"
#include "proc.h"

//extern void hal_init_early(void);


adr_t bootimage_start = 0; //This should be a phys address
adr_t bootimage_end = 0; //This should be a phys address

extern void hal_bp_init(void);
extern void hal_pci_init(void);
extern void hal_start_scheduling(void);
extern void hal_start_smp(void);

extern void net_init(void);
extern void net_poll(void);
extern void net_test_arp(void);

/*
    When kmain is called.
        * log_init has been called to set *log_putchar_ptr function pointer
        * ....
        * ....
 */


//GCC och Clang skapar automatiskt två dolda symboler för din anpassade sektion: __start_SEKTIONSNAMN och __stop_SEKTIONSNAMN
extern const driver_annotation_t __start_driver_annotation;
extern const driver_annotation_t __stop_driver_annotation;

extern const usbdriver_annotation_t __start_usbdriver_annotation;
extern const usbdriver_annotation_t __stop_usbdriver_annotation;


void register_drivers(void) {
    log_msg("Register drivers\n");

    // Sätt pekare till start och slut
    const driver_annotation_t *start = &__start_driver_annotation;
    const driver_annotation_t *stop = &__stop_driver_annotation;

    // Loopa igenom minnesblocket som en array
    for (const driver_annotation_t *d = start; d < stop; d++) {
        log_msg("Register driver - %s \n", d->name);
        hwdb_driverdb_add((driver_t*) d->driver);
    }
}


void register_usb_drivers(void) {
    log_msg("Register USB drivers\n");

    // Sätt pekare till start och slut
    const usbdriver_annotation_t *start = &__start_usbdriver_annotation;
    const usbdriver_annotation_t *stop = &__stop_usbdriver_annotation;

    // Loopa igenom minnesblocket som en array
    for (const usbdriver_annotation_t *d = start; d < stop; d++) {
        log_msg("Register USB driver - %s \n", d->name);
        usb_register_driver((struct usb_device_driver *) d->driver);
        //hwdb_driverdb_add((driver_t*) d->driver);
    }
}


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

    hwdb_devicedb_init();
    hwdb_driverdb_init();
    hal_pci_init();
    register_drivers();
    pci_config_enumerate_regions();
    hwdb_devicedb_debug_listdevices();
    //hwdb_devicedb_debug_listdrivers();
    pci_config_driver_init();


    log_msg("***********************************************************************\n");
    log_msg("*                    START SCHEDULING!                                *\n");
    log_msg("***********************************************************************\n");
    hal_start_scheduling();


/*
    //test USB
    log_msg("***********************************************************************\n");
    log_msg("*                    START USB Subsystem!                             *\n");
    log_msg("***********************************************************************\n");
    register_usb_drivers();
    usb_init();
*/

    //log_msg("***********************************************************************\n");
    //log_msg("*                    START Network Subsystem!                         *\n");
    //log_msg("***********************************************************************\n");
    //net_init();



    log_msg("***********************************************************************\n");
    log_msg("*                    START SMP!                                       *\n");
    log_msg("***********************************************************************\n");
    hal_start_smp();

    //cpptest_test();

    //shutdown Qemu
    //requires qemu to be started with
    //-device isa-debug-exit,iobase=0xf4,iosize=0x04
    //-no-shutdown must not be used (otherwise anm excpetion will be thrown)

    //net_test_arp();
    cpu_t* cpu = CURRENTCPU;

    //Simulate work......
    int i=0;
    while(i<50){
        i+=1;
        log_msg("CPU[%d] WORK (%d)\n",cpu->cpuid,i);
        //usb_poll();
        //net_poll();
        mdelay(1000);
        //HANG HERE
    }


    //Kmain setup is finished leave rest to started tasks....

    proc_t* idleproc = cpu->currentproc;
    cpu->idleproc = idleproc;   // dont set as idleproc until all setup is done, otherwise it will be starved when starting other kerneltasks.....


    i=0;
    while(i<5000){
        i+=1;
        log_msg("CPU[%d].... NO WORK (%d)\n",cpu->cpuid,i);
        mdelay(100);
        //HANG HERE
    }


    io_outb(0xf4, 0x00);
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