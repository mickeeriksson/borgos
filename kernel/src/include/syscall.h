#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "cpu.h"

// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16    //ssize_t   write(int, const void *, size_t);
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21

#define MAXSYSCALL 21
#define MAXSYSARG 10

extern void syscall_enter(trapframe_t* tf);
extern void syscall_lnx_enter(trapframe_t* tf);

extern int cpu_syscall_num(trapframe_t* tf);
extern int cpu_syscall_arg64(trapframe_t* tf,int n,uint64_t* regp);
extern int cpu_syscall_argptr(trapframe_t* tf,int n, char **pp, int size);

extern reg_t sys_write(trapframe_t* tframe);



#endif


