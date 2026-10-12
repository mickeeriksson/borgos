

.section .startinit, "ax"
.code64

.global startinit_start
.global startinit_end
.align 4096

startinit_start:

	nop
    nop
    push %rax
1:
    nop
    nop
    //mov $1, %rax
    //int $0x81
    mov $16, %eax         # SYS_write
    mov $1, %ebx          # fd = stdout
    lea msg, %ecx         # buffer
    mov $12, %edx         # längd
    int $0x81
    nop
    nop
    jmp 1b

msg: .ascii "STARTINIT!\n\0"

startinit_end:
