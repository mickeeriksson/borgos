#include "cpu.h"
#include "cpu/cpu_cpu.h"

void arch_cpu_debugprint(cpu_t *cpup) {
    log_msg("CPU INFO cpu=%d\n", cpup->cpuid);

    struct arch_cpu_info* acpu = &cpup->archcpu;
    log_msg("    xapic_logicalid   = %d\n", acpu->xapic_logicalid);
    log_msg("    vendor            = %s\n", acpu->cpuid_vendorstring);
    log_msg("    max cpuid leafs   = %d\n", acpu->cpuid_maxbasicleafs);
    log_msg("    cpuid_feature_ebx = 0x%x\n", acpu->cpuid_feature_ebx);
    log_msg("    cpuid_feature_ecx = 0x%x\n", acpu->cpuid_feature_ecx);
    log_msg("    cpuid_feature_edx = 0x%x\n", acpu->cpuid_feature_edx);
    log_msg("\n");
    log_msg("    cpuid_feature APIC      = %d\n", (acpu->cpuid_feature_edx & CPUID_FEAT_EDX_APIC)>>9);



}