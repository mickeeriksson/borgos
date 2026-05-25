//https://github.com/eteran/os64/blob/master/arch/x86_64/boot.S
//#include "./include/mach/mconfig.h"

#define KERNEL_UPPER_HALF_OFFSET  0xFFFF800000000000
//#define V2P(x) ((x)-KERNEL_UPPER_HALF_OFFSET)

.equ COM1, 0x3F8

.macro portoutb port=0, offset=0, value=0
    mov \value,%al
    mov \port,%dx
    add \offset,%dx
    outb %al,%dx
.endm

//Tell assembler not to use executable stack
.section .note.GNU-stack,"",@progbits

.section .multiboot2
.align 8
//.long MB2MAGIC
mb2hdr_start:
.long 0xE85250D6       /* 'magic number' lets bootloader find the header */
//.long MB2ARCH
.long 0x0             /* means i386 32bit protected mode*/
//.long MB2LEN
.long (mb2hdr_end - mb2hdr_start)  //header len
//.long MB2CHECKSUM
.long 0x100000000 - (0xe85250d6 + 0 + (mb2hdr_end - mb2hdr_start))
//; insert optional multiboot tags here


//FB TAG
.word 0x0005        //FB tag
.word 0x0000        //FB falgs
.long 20        //FB size
.long 800        //FB width
.long 600        //FB height
.long 32          //FB depth


.align 8
// end tag
.long 0x00000000    //end tag, type & flags
.long 0x00000008    //end tag, size
mb2hdr_end:


/*
The linker script specifies _start as the entry point to the kernel and the
bootloader will jump to this position once the kernel has been loaded. It
doesn't make sense to return from this function as the bootloader is gone.
*/
.section .text.boot
.global _start
//.align 32
.align 32
.code32

.macro dbg32 c1,c2
    portoutb $COM1, $0, \c1
    portoutb $COM1, $0, \c2
    portoutb $COM1, $0, ','
.endm


_start:
    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint

    /*
    	The bootloader has loaded us into 32-bit protected mode on a x86
    	machine. Interrupts are disabled. Paging is disabled. The processor
    	state is as defined in the multiboot standard. The kernel has full
    	control of the CPU. The kernel can only make use of hardware features
    	and any code it provides as part of itself. There's no printf
    	function, unless the kernel provides its own <stdio.h> header and a
    	printf implementation. There are no security restrictions, no
    	safeguards, no debugging mechanisms, only what the kernel provides
    	itself. It has absolute and complete power over the
    	machine.
    	*/


    /*
        trick to get relative adressing in ecx, this is since we cant read value of EIP directly?
    */
//call base
base:
        //pop %ecx

    	/*
    	To set up a stack, we set the esp register to point to the top of the
    	stack (as it grows downwards on x86 systems). This is necessarily done
    	in assembly as languages such as C cannot function without a stack.
    	*/
        //mov $V2P(astack_top),%esp
    mov $astack_top,%esp


    push $0x0    // fake saved return value. (terminate stacktrace)
    mov %esp, %ebp
    push $0x0    // saved base ptr

    // Save ebx & eax to stack, used for call to mach_init_hw later on...
    push %ebx    // ARG2=pointer to multiboot structure
    push %eax    // ARG1=magic value

    call setupcom1
    movl $(0x2f<<24) | ('T' << 16) | (0x2f<<8) | ('S'),(0xb8000+80*0*2)
    portoutb $COM1, $0, $('S')
    portoutb $COM1, $0, $('T')
    portoutb $COM1, $0, $(',')

    call check_cpuid
    call check_long_mode
    //call setup_page_tables_2mb
    call setup_page_tables_2mb_4gb

    //call check_hugh_pages
    //call setup_page_tables_1gb

    call enable_paging

    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint
    //jump to long mode.
    mov $enter_long_mode,%eax
    //jmp $0x08,$V2P(enter_long_mode)
    //jmp $0x08,$enter_long_mode
    jmp $0x10,$enter_long_mode




setupcom1:
portoutb $COM1,$1,$0x00  // PORT+1=0x00, Disable all interrupts
portoutb $COM1,$3,$0x80  // PORT+3=0x80 ,Enable DLAB (set baud rate divisor)
portoutb $COM1,$0,$0x01  // PORT+0=0x01 ,Set divisor to 3 (lo byte) 115200 baud
portoutb $COM1,$1,$0x00  // PORT+1=0x00 ,                 (hi byte)
portoutb $COM1,$3,$0x03  // PORT+3=0x03 ,8 bits, no parity, one stop bit
portoutb $COM1,$2,$0xC7  // PORT+2=0xC7 ,Enable FIFO, clear them, with 14-byte threshold
portoutb $COM1,$4,$0x00  // PORT+4=0x00 ,IRQs disabled,
ret

// Prints `ERR: ` and the given error code to screen and hangs.
// parameter: error code (in ascii) in al
error:
    movl $0x4f524f45, (0xb8000)
    movl $0x4f3a4f52, (0xb8004)
    movl $0x4f204f20, (0xb8008)
    movb %al, (0xb800a)
    movb %al, %bl
    portoutb $COM1, $0, $('E')
    portoutb $COM1, $0, $('R')
    portoutb $COM1, $0, $('R')
    portoutb $COM1, $0, $(':')
    portoutb $COM1, $0, %bl
    portoutb $COM1, $0, $(',')
    hlt

// Throw error 1 if the CPU doesn't support the CPUID command.
check_cpuid:
    pushf                // Store the FLAGS-register.
    pop %eax             // Restore the A-register.
    mov %eax, %ecx       // Set the C-register to the A-register.
    xor $1 << 21, %eax   // Flip the ID-bit, which is bit 21.
    push %eax            // Store the A-register.
    popf                 // Restore the FLAGS-register.
    pushf                // Store the FLAGS-register.
    pop %eax             // Restore the A-register.
    push %ecx            // Store the C-register.
    popf                 // Restore the FLAGS-register.
    xor %ecx, %eax       // Do a XOR-operation on the A-register and the C-register.
    jz no_cpuid          // The zero flag is set, no CPUID.
    ret                  // CPUID is available for use.
no_cpuid:
    mov $'1', %al
    jmp error

// Throw error 2 if the CPU doesn't support Long Mode.
check_long_mode:
    movl $0x80000000, %eax  // Set the A-register to 0x80000000.
    cpuid                   // CPU identification.
    cmp $0x80000001, %eax   // Compare the A-register with 0x80000001.
    jb no_long_mode         // It is less, there is no long mode.
    movl $0x80000001, %eax  // Set the A-register to 0x80000001.
    cpuid                   // CPU identification.
    test $1 << 29, %edx     // Test if the LM-bit, which is bit 29, is set in the D-register.
    jz no_long_mode         // They aren't, there is no long mode.
    ret
no_long_mode:
    mov $'2', %al
    jmp error


// Throw error 3 if the CPU doesn't support Huge pages
check_hugh_pages:
    movl $0x80000000, %eax  // Set the A-register to 0x80000000.
    cpuid                   // CPU identification.
    cmp $0x80000001, %eax   // Compare the A-register with 0x80000001.
    jb no_hugh_pages         // It is less, there is no long mode.
    movl $0x80000001, %eax  // Set the A-register to 0x80000001.
    cpuid                   // CPU identification.
    test $1 << 26, %ebx     // Test if the pdpe1gb-bit, which is bit 26, is set
    jz no_hugh_pages         // They aren't, there is no long mode.
    ret
no_hugh_pages:
    mov $'3', %al
    jmp error


//setup page tables with 2mb pages
/*setup_page_tables_2mb:
    movl $(0x2f<<24) | ('1' << 16) | (0x2f<<8) | ('P'),(0xb8000+80*1*2)
    portoutb $COM1, $0, $('P')
    portoutb $COM1, $0, $('1')
    portoutb $COM1, $0, $(',')
    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint

    //l4_tbl[0]=&l3_tbl
    mov $l3_tbl,%eax
    or $0x03,%eax            // bit [1:0] present + writable
    movl %eax, (l4_tbl)  //l4_tbl[0]=&l3_tbl
    mov $256,%ecx       //upper half 512gig
    movl %eax, l4_tbl(,%ecx,8)  //l4_tbl[256]=&l3_tbl

    mov $l2_tbl,%eax
    or $0x03,%eax            // bit [1:0] present + writable
    movl %eax, (l3_tbl)  //l3_tbl[0]=&l2_tbl

    //map each p2 entry to a big 2mb page.
    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint
    mov $0, %ecx
.map_l2_2mb_page:
    mov $0x200000,%eax        //2Mb
    mul %ecx
    or $(0x03 | (1<<7)),%eax            // bit [1:0] present + writable, bit[7] = huge
    movl %eax, l2_tbl(,%ecx,8)     //l2_tbl[ecx]=mem

    inc %ecx
    cmp $0x04,%ecx
    jne .map_l2_2mb_page
    ret
*/

//setup page tables with 2mb pages
setup_page_tables_2mb_4gb:
    movl $(0x2f<<24) | ('1' << 16) | (0x2f<<8) | ('P'),(0xb8000+80*1*2)
    portoutb $COM1, $0, $('P')
    portoutb $COM1, $0, $('1')
    portoutb $COM1, $0, $(',')
    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint

    //l4_tbl[0]=&l3_tbl
    mov $l3_tbl,%eax
    or $0x03,%eax            // bit [1:0] present + writable
    movl %eax, (l4_tbl)  //l4_tbl[0]=&l3_tbl
    mov $256,%ecx       //upper half 512gig
    movl %eax, l4_tbl(,%ecx,8)  //l4_tbl[256]=&l3_tbl

    mov $l2_tbl,%eax
    or $0x03,%eax            // bit [1:0] present + writable
    movl %eax, (l3_tbl)  //l3_tbl[0]=&l2_tbl

    mov $1,%ecx       //next table
    add $4096,%eax
    movl %eax, l3_tbl(,%ecx,8)  //l3_tbl[1]=&l2_tbl + 4096

    mov $2,%ecx       //next table
    add $4096,%eax
    movl %eax, l3_tbl(,%ecx,8)  //l3_tbl[2]=&l2_tbl + 4096*2

    mov $3,%ecx       //next table
    add $4096,%eax
    movl %eax, l3_tbl(,%ecx,8)  //l3_tbl[3]=&l2_tbl + 4096*3



    //map each p2 entry to a big 2mb page.
    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint
    mov $0, %ecx
.map_l2_2mb_page_4:
    mov $0x200000,%eax        //2Mb
    mul %ecx
    or $(0x03 | (1<<7)),%eax            // bit [1:0] present + writable, bit[7] = huge
    movl %eax, l2_tbl(,%ecx,8)     //l2_tbl[ecx]=mem

    inc %ecx
    cmp $2048,%ecx                      //2048*2mb = 4gb
    jne .map_l2_2mb_page_4
    ret



enable_paging:
    movl $(0x2f<<24) | ('2' << 16) | (0x2f<<8) | ('P'),(0xb8010+80*1*2)
    portoutb $COM1, $0, $('P')
    portoutb $COM1, $0, $('2')
    portoutb $COM1, $0, $(',')
    //load &l4_tbl to cr3
    //mov $V2P(l4_tbl),%eax
    mov $l4_tbl,%eax
    mov %eax,%cr3

    //enable PAE -flag
    mov %cr4, %eax
    or $(1<<5),%eax             //bit[5] = ???
    mov %eax,%cr4

    //; set the long mode bit in the EFER MSR (model specific register)
    mov $0xC0000080,%ecx
    rdmsr
    or $(1<<8), %eax
    wrmsr

    //enable paging, by setting bit 31 in CR0
    mov %cr0,%eax
    or $(1<<31),%eax
    mov %eax,%cr0
    movl $(0x2f<<24) | ('3' << 16) | (0x2f<<8) | ('P'),(0xb8020+80*1*2)
    portoutb $COM1, $0, $('P')
    portoutb $COM1, $0, $('3')
    portoutb $COM1, $0, $(',')


    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint
    //Set new 64 bit GDT
    movl $(0x2f<<24) | ('1' << 16) | (0x2f<<8) | ('G'),(0xb8000+80*2*2)
    portoutb $COM1, $0, $('G')
    portoutb $COM1, $0, $('1')
    portoutb $COM1, $0, $(',')
    //lgdt (V2P(.GDTPTR))
    lgdt (.GDTPTR)
    movl $(0x2f<<24) | ('2' << 16) | (0x2f<<8) | ('G'),(0xb8010+80*2*2)
    portoutb $COM1, $0, $('G')
    portoutb $COM1, $0, $('2')
    portoutb $COM1, $0, $(',')
    ret



.code64
enter_long_mode:
    //; load 0x10 (DATA) into all data segment registers
    mov $0x20,%rax
    mov %rax,%ss
    mov %rax,%ds
    mov %rax,%es
    mov %rax,%fs
    mov %rax,%gs

    cli;

    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoin
    movabs $enter_upper_high,%rax
    jmp *%rax


// Have to declare these here and not in C otherwise it will be placed in bss and cleared later on
.balign 8
//bootmem_start:
//    .quad 0            /* 8 bytes */
//bootmem_end:
//    .quad 0            /* 8 bytes */
//boot_pgd_ptr:
//    .quad 0            /* 8 bytes */
.global _bootinfosignature
_bootinfosignature:
    .quad 0            /* 8 bytes */
.global _bootinfoaddr
_bootinfoaddr:
    .quad 0            /* 8 bytes */

.global GDT64
GDT64:
.NULLSEG:
    .set GDTIDX_NULLSEG, (.NULLSEG - GDT64)
    .quad 0         //8bytes null seg
    .quad 0         //8bytes null seg
.KCODE64:
    .set GDTIDX_KCODE64, (.KCODE64 - GDT64)
    .long 0xFFFF    //base + limit  bit 31:0
    .byte 0x0       // base mid bit 16-23
    .byte ((1<<1) | (1<<3) | (1<<4) | (1<<7) )     //access present(7), notsys(4), exec(3),rw(1)
    .byte ((1<<7) | (1<<5) | 0xF)     //flags 4klimit(7), sz32(6), longmode(5) + limit
    .byte 0x0       //base high bit 24-32
    .quad 0         //8bytes null seg
.KDATA64:
    .set GDTIDX_KDATA64, (.KDATA64 - GDT64)
    .long 0xFFFF    //base + limit  bit 31:0
    .byte 0x0       // base mid bit 16-23
    .byte ((1<<1) | (1<<4) | (1<<7) )     //access present(7), notsys(4), exec(3),rw(1)
    .byte ((1<<7) | (1<<6) | 0xF)     //flags 4klimit(7), sz32(6), longmode(5) + limit
    .byte 0x0       //base high bit 24-32
    .quad 0         //8bytes null seg
.GDTPTR:
    .word (. - GDT64 - 1)        //size of GDT table
    .quad GDT64


    /*
    The multiboot standard does not define the value of the stack pointer register
    (esp) and it is up to the kernel to provide a stack. This allocates room for a
    small stack by creating a symbol at the bottom of it, then allocating 16384
    bytes for it, and finally creating a symbol at the top. The stack grows
    downwards on x86. The stack is in its own section so it can be marked nobits,
    which means the kernel file is smaller because it does not contain an
    uninitialized stack. The stack on x86 must be 16-byte aligned according to the
    System V ABI standard and de-facto extensions. The compiler will assume the
    stack is properly aligned and failure to align the stack will result in
    undefined behavior.
    */
//    .section .bss
/*.align 4096
    skipl4_tbl:
        .skip 4096 # 4 KiB
    skipl3_tbl:
        .skip 4096 # 4 KiB
    skipl2_tbl:
        .skip 4096 # 4 KiB
*/

.align 4096
    l4_tbl:
        .skip 4096 //# 4 KiB
    l3_tbl:
        .skip 4096 //# 4 KiB
    l2_tbl:
        .skip 4096 //# 4 KiB
        .skip 4096 //# 4 KiB
        .skip 4096 //# 4 KiB
        .skip 4096 //# 4 KiB

    .align 16
.global astack_bottom
    astack_bottom:
    .skip 16384 //# 16 KiB
    .skip 16384 //# 16 KiB            //need a lot of stack due to init of memoryzonemap....
//    .skip 16384 # 16 KiB
//    .skip 16384 # 16 KiB
//    .skip 4096 # 4 KiB
.global astack_top
    astack_top:


.section .text.bootupperhigh
enter_upper_high:
    movl $(0x2f<<24) | ('1' << 16) | (0x2f<<8) | ('U'),(0xb8000+80*3*2)
    portoutb $COM1, $0, $('U')
    portoutb $COM1, $0, $('1')
    portoutb $COM1, $0, $(',')

    //jmp enter_upper_high

    //change stack ptr to upper_half
    mov $KERNEL_UPPER_HALF_OFFSET,%rax
    add %rax,%rsp
    add %rax,%rbp


    //restore multiboot args from stack
    pop %rax            //will contain arg1,arg2 as 32 values from 32bit pushed mode
    mov %rax,%rsi     // ARG2=pointer to multiboot structure
    shr $32,%rsi
    mov %rsi, _bootinfoaddr
    //pop %esi
    mov %rax,%rdi     // ARG1=magic value
    mov $0xFFFFFFFF,%rax
    and %rax,%rdi
    mov %rdi, _bootinfosignature

    //push stack and basepointer to terminate stacktrace here.
    push $0x0    // fake saved return value. (terminate stacktrace)
    mov %esp, %ebp
    push $0x0    // saved base ptr

    //call EXT_C(mach_init_hw)
    //call mach_init_hw
    //call kmain_enter_multiboot2
    //call hal_init_with_multiboot2
    //xchg	%bx, %bx	 	//; Enter debug through magic Bochs breakpoint


    //call kmain
    call hal_start

    cli
1:	hlt
    jmp 1b
