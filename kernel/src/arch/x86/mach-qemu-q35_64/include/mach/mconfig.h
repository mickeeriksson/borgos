#ifndef _MACH_CONFIG_H_
#define _MACH_CONFIG_H_

//#include "mconfig.h"

#define  CPU_X86_64

//#define NOMMU   1                                       //SKIP MMU
#undef   NOMMU                                          //USE MMU, default

//#define  SMP

#define KERNEL_PHYS_BASE          0x100000              // Physical, Kernel is loaded to 1Mb physical
#define BOOT_ENTRY                KERNEL_PHYS_BASE      // Boot Entry, used in linker
#ifdef NOMMU
    #define KERNEL_UPPER_HALF_OFFSET  0x0               // Virt = phys, when no MMU is used
#else
    //#define KERNEL_UPPER_HALF_OFFSET  0x0        // upper half, Virtual offsett
    #define KERNEL_UPPER_HALF_OFFSET  0xFFFF800000000000        // upper half, Virtual offsett
#endif //NOMMU


#ifdef NOMMU
    // Unable to spin up multi CPU with MMU disabled, since spinlocks doesnt work without MMU enabled
    #undef MAXCPU
    #define MAXCPU        1
    #undef SMP
#endif

//Include other code-chunks
#define MULTIBOOT                                       //Use Multiboot headers (GRUB)

#define BOOTMEM_STARTMB       16                        // Dont use 16mb DMA for bootmem, ie start botmem at 16mb-adress
#define MAXBOOTPGD_PFN (1048576-1)                      //max mem mmu mappen in boot.s, 4gb OBS utger maxPFN INTE antal pages därav -1 efter pfn startar på 0.


#define CONFIG_PIC_DISABLE                1
#define CONFIG_ACPI               1
#define CONFIG_APIC               1
#define MAXAPIC_LOGICAL_CPUID     256                  //Max logical cpu id's,

#define IRQ_STARTVECTOR           32            //map IRQ0 to interruptvector 32

#endif