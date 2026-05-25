
.altmacro

.macro ISR_NOERRORCODE  isrno
.global isr\isrno
.balign 4
isr\isrno:
    push   $0x00000000     //dummy a dummy errorcode
    push   $\isrno           //push the interrupt no
    jmp isr_common
.endm

.macro ISR_WITHERRORCODE  isrno
.global isr\isrno
.balign 4
isr\isrno:
    push  $\isrno           //only push interrupt number, error code has already been pushed
    jmp isr_common
.endm

//; This macro creates a stub for an IRQ - the first parameter is
//; the IRQ number, the second is the ISR number it is remapped to.
.macro IRQ  irqno  isrno
.global irq\irqno
.balign 4
irq\irqno:
	push $\irqno
	push $\isrno
	jmp isr_common
.endm

.set i,0
.rept 256
    .if i==8 || i==10 || i==11 || i==12 || i==13  || i==14 || i==17 || i==21
        //exceptions with error code
        ISR_WITHERRORCODE %i
    .elseif i<=31
        //exceptions without error code
        ISR_NOERRORCODE %i
    .elseif i>=32 && i<=47
        //.set irqnum,i-32
        //IRQ %(i-32),%i
        ISR_NOERRORCODE %i
    .else
    .endif
    .set i,i+1
.endr


.macro stubentry isrno
    .quad isr\isrno
.endm

.global isr_stub_table
isr_stub_table:
.set i,0
.rept 256
    .if i==8 || i==10 || i==11 || i==12 || i==13  || i==14 || i==17 || i==21
        stubentry %i
    .elseif i<=31
        stubentry %i
    .elseif i>=32 && i<=47
        stubentry %i
    .else
        .quad 0
    .endif
    .set i,i+1
.endr

isr_common:
    //# 1. Spara CPU-register (Context Save)
    //pushq $42 //dummy
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rdi

    pushq %rsi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10

    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15



    //OBS! Make sure stackframe is always a size of 16bytes, so that alignment is 16bytes.
    //stacke frame is 22*8 (=11*16) so that is OK!


    //# 2. Skicka stackpekaren som argument till C-funktionen (RDI = första argumentet)
    movq %rsp, %rdi
    //# 3. Anropa C-koden
    call isr_generic

    # 4. Återställ CPU-register
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rsi
    popq %rdi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    //städa dummy
    //addq $8, %rsp

    //# 5. Städa upp avbrottsnummer och felkod
    addq $16, %rsp

    //# 6. Återgå från avbrott
    iretq
