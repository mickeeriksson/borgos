#ifndef _CPU_BARRIER_H_
#define _CPU_BARRIER_H_

// Compiler barrier.h. x86 DMA is cache coherent, so no cache maintenance is
// needed; ordering against the compiler is. See AD-5 and ARCHITECTURE.md
// section 3.
static inline void cpu_barrier(void) {
    __asm__ volatile ("" ::: "memory");
}


#endif