#include "syscall.h"
#include "log.h"
#include "cpu.h"
#include "error.h"
#include "types.h"

static reg_t (*syscalls[])(trapframe_t* tf) = {
    [SYS_fork]  =  0,
    [SYS_exit]  =  0,
    [SYS_wait]  =  0,
    [SYS_pipe]  =  0,
    [SYS_read]  =  0,
    [SYS_kill]  =  0,
    [SYS_exec]  =  0, //sys_exec,
    [SYS_fstat] =  0,
    [SYS_chdir] =  0,
    [SYS_dup]   =  0,
    [SYS_getpid]=  0,
    [SYS_sbrk]  =  0,
    [SYS_sleep] =  0,
    [SYS_uptime]=  0,
    [SYS_open]  =  0,
    [SYS_write] =  sys_write, //sys_write,
    [SYS_mknod] =  0,
    [SYS_unlink]=  0,
    [SYS_link]  =  0,
    [SYS_mkdir] =  0,
    [SYS_close] =  0,
};


void syscall_enter(trapframe_t* tf)
{
    log_msg("Syscall enter\n");
    log_msg("syscalls = %#lx\n", syscalls);

    cpu_t* cpup = CURRENTCPU;
    proc_t *curprocp = cpup->currentproc;

    int num = cpu_syscall_num(tf);
    log_msg("syscall num = %d   @ %#lx\n", num,syscalls[num]);

    reg_t ret = -1;


    if(num > 0 && num <= MAXSYSCALL && syscalls[num]) {
        ret = syscalls[num](tf);
        log_msg("ret = %#lx\n", ret);
    } else {
        log_msg("%d %s: unknown sys call %d\n", curprocp->pid, curprocp->name, num);
        PANIC("SYSCALL NOT IMPLEMENTED \n");
        log_msg("ret = %#lx\n", ret);
        ret = -1;
    }


    /*
    int num;
    struct proc *curprocp = curproc();

    num = curprocp->tf->eax;
    if(num > 0 && num <= MAXSYSCALL && syscalls[num]) {
        curprocp->tf->eax = syscalls[num]();
    } else {
        log_msg("%d %s: unknown sys call %d\n", curprocp->pid, curprocp->name, num);
        PANIC("SYSCALL NOT IMPLEMENTED \n");
        curprocp->tf->eax = -1;
    }
    //bochs_brk();
    */
}


void syscall_lnx_enter(trapframe_t* tf)
{
    log_msg("Linux 32-bit Syscall enter\n");

    /*
    int num;
    struct proc *curprocp = curproc();

    num = curprocp->tf->eax;
    if(num > 0 && num <= MAXSYSCALL && syscalls[num]) {
        curprocp->tf->eax = syscalls[num]();
    } else {
        log_msg("%d %s: unknown sys call %d\n", curprocp->pid, curprocp->name, num);
        PANIC("SYSCALL NOT IMPLEMENTED \n");
        curprocp->tf->eax = -1;
    }
    //bochs_brk();
    */
}