#include "error.h"
#include "cpu.h"
#include "cpu.h"
#include "driver/vsystimer.h"
#include "cpu/cpu_getcpu.h"

extern int arch_cpu_hasfeature_RDTSC(void);

uint64_t cputimer_getticks64(void) {
    //OBS different on x86 and x86_64
    uint32_t low, high;
    __asm__ __volatile__(
        "rdtsc"
        : "=a" (low), "=d" (high) // '=a' mappar till eax/rax, '=d' mappar till edx/rdx
    );
    return ((uint64_t)high << 32) | low;
}

void cputimer_initcpu_hz(void) {
    log_msg("Init cputimer calc HZ\n");

    cpu_t* cpu = CURRENTCPU;

    if (arch_cpu_hasfeature_RDTSC()==0) {
        PANIC("CPU does not support RDTSC (TSC feature)");
    }

    uint64_t endtick = vsystimer_millis() + 1000;

    uint64_t start = cputimer_getticks64();
    // Hindrar BÅDE Clang och x86-processorn från att ändra ordningen
    __asm__ __volatile__("mfence" : : : "memory");
    while (vsystimer_millis() < endtick) {
        // do nothing
    }
    // Hindrar BÅDE Clang och x86-processorn från att ändra ordningen
    __asm__ __volatile__("mfence" : : : "memory");
    uint64_t end = cputimer_getticks64();

    uint64_t lcpuhz = (end - start)*1;
    log_msg("LCPU hz: 0x%lx (%d)\n", lcpuhz,lcpuhz);

    cpu->archcpu.cputimer_hz = lcpuhz;

    //PANIC("TODO");
}

void cputimer_delayticks(uint64_t ticks) {
    uint64_t timeout = cputimer_getticks64() + ticks;
    while (cputimer_getticks64() < timeout) {
    }
}

void cputimer_mdelay(uint64_t millis) {
    cpu_t* cpu = CURRENTCPU;
    uint64_t cpuhz = cpu->archcpu.cputimer_hz;
    uint64_t ticks = (millis * cpuhz) / 1000;
    cputimer_delayticks(ticks);
}