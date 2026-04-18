#ifndef _DEFAULTCONFIG_H_
#define _DEFAULTCONFIG_H_


/*
#define NOMMU   1                                       //SKIP MMU
//#define NOMMU                                         //USE MMU, default

#define KERNEL_PHYS_BASE          0x100000              // Physical, Kernel is loaded to 1Mb physical
#define BOOT_ENTRY                KERNEL_PHYS_BASE      // Boot Entry, used in linker
#ifdef NOMMU
    #define KERNEL_UPPER_HALF_OFFSET  0x0               // Virt = phys, when no MMU is used
#else
    #define KERNEL_UPPER_HALF_OFFSET  0xC0000000        // upper half, Virtual offsett
#endif //NOMMU


//Include other code-chunks
#define MULTIBOOT                                       //Use Multiboot headers (GRUB)

#define BOOTMEM_PRESERVE16M_DMA       1                 // Dont use 16mb DMA for bootmem, ie start botmem at 16mb-adress
*/



#define BOOTMEM_ROUNDUP               0x100000          // roundup bootmem to nearest 1M, set this to 4k, tio wate less memory.

//#define PAGESIZE  0x1000 //4kb
#define PAGESHIFT  12      //pagesize = 4kb



//Memorymapping policies
#define MEMMAP_POLICY_CONTIG
//#define MEMMAP_CONTIG_OFFSET


#define USE_CPU_LOCAL_TIMER         1                   //Use CPU local timer.

//#define BOOT_ENTRY      0x80000                 // Boot_entry address
//#define KERNEL_UH_BASE  0xFFFFFFC000000000    // upper half
//#define USER_STACK_TOP  0xFF00000000000000    // Top of stack in user proc
//#define KERNEL_VIRTUAL_BASE 0xC0000000      // upper half
//#define VFB_BASE        0xFFFFFFC100000000    //- 0xFFFFFFC200000000 //virtual frambuffer, at 4Gb memory above kernel_base
//#define VFB_SIZE        (1920*1280*4)         //9mb

//#define PAGESIZE  0x1000 //4kb
//#define RESERV_TOPMEMORY  (1024*1024) //1Mb
//#define PAGESHIFT 12  // shift virtaddr right by 12 to get page #

//#define NRPHYSPAGES  66048 //(0x0 - 0x101FF FFF)


//#define SYS_CTRLREG_BASE                    0x10000000
//#define SYS_FLAGS                           (SYS_CTRLREG_BASE+0x0030)
//#define SYS_FLAGSSET                        (SYS_CTRLREG_BASE+0x0030)

//#define PERIPHERAL_BASE           0x1F000000              //maybe should be REALVIEWPBA_PERBASE

//#define VECTORTABLE_BASE          0xFFFF0000

#define MAXCPU    4
//#define MAXPROC  16

//Ticktimer
//#define TICKTIMER_HZ 10
//#define TICKTIMER_MSPERTICK (1000/TICKTIMER_HZ)


//#define KERNEL_STACK_SIZE 4096

//#define MACH_MEMSIZE (2*1024*1024)  //4meg
//#define MACH_MEMSIZE (1024*1024+4096*36)  //4meg
//#define MACH_IOPORTSTART (0x10000000)

//#define MACH_MEMSIZE (16*1024*1024)  //16meg
#define ALLOCBUDDY_MAXORDER   6     // 2^6 * 4096 = 256kb
//#define USEBUDDY 1

//define HAS_SHARED_PAGETABLES   0     // IF cpu has 0=single (shared pagetable), or 1=split pagetable (user and kernel) as aarch64

#define VFS_MAXP_OFILE     256      //antal öppna filer per process,
//#define VFS_NAME_MAX       255	    // # chars in a file name
//#define VFS_USEEXT2 1


#endif //_DEFAULTCONFIG_H_