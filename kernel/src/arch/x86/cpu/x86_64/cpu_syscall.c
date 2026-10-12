#include "cpu.h"
#include "error.h"

//fetch the n-th argument
int cpu_syscall_arg64(trapframe_t* tf,int n,uint64_t* regp){
    if (n>6) {
        PANIC("TODO implement argument fetching from stack");
    }
    if (n==1) {
        *regp = tf->rbx;
    }
    if (n==2) {
        *regp = tf->rcx;
    }
    if (n==3) {
        *regp = tf->rdx;
    }
    if (n==4) {
        *regp = tf->rsi;
    }
    if (n==5) {
        *regp = tf->rdi;
    }
    if (n==6) {
        *regp = tf->rbp;
    }
    return 0;
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.
// Check that the pointer lies within the process address space.
int cpu_syscall_argptr(trapframe_t* tf,int n, char **pp, int size)
{
    uint64_t i;
    //struct proc *curprocp = curproc();

    if(cpu_syscall_arg64(tf,n, &i) < 0)
        return -1;
    //TODO
    //if(size < 0 || (uint32_t)i >= curprocp->pmemsize || (uint32_t)i+size > curprocp->pmemsize)
    //    return -1;
    *pp = (char*)i;
    return 0;
}

int cpu_syscall_num(trapframe_t* tf) {
    return tf->rax;
}