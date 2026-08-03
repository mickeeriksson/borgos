
.altmacro


#define TASK_RSP_OFFSET           0
#define TASK_CR3_OFFSET           8


.section .text
.global cpu_swapcontext
//.type cpu_swapcontext, @function

cpu_swapcontext:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    pushfq
    mov %rsp, TASK_RSP_OFFSET(%rdi)    # spara gamla tråden

    # --- adressrymdsbyte ---
    mov TASK_CR3_OFFSET(%rsi), %rax
    //cmp current_cr3(%rip), %rax
    //je  1f                              # samma adressrymd — undvik onödig TLB-flush
    mov %rax, %cr3                      # laddar nytt PML4, flushar all icke-global TLB
    //mov %rax, current_cr3(%rip)
//1:
    mov TASK_RSP_OFFSET(%rsi), %rsp
    popfq
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    ret


.global cpu_kerneltask_trampoline
cpu_kerneltask_trampoline:
    mov %r12, %rdi       # entry-pekaren låg smugglad i r12
    mov %r13, %rsi       # arg låg smugglad i r13 — blir första parametern per System V
    call scheduler_start_kerneltask
