#include "cpu.h"
#include "cpu/cpu_cpu.h"

extern struct cpustate *apic_id_cpumap[MAXAPIC_LOGICAL_CPUID];

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


void arch_cpu_init(int cpuid) {
    log_msg("\n\n\nGET SOME INFO ABOUT CPU\n\n");

    cpu_init_cpustate(cpuid);

    cpu_t *cpup = &cpu[cpuid];
    cpup->cpuid=cpuid;
    struct arch_cpu_info* acpu = &cpup->archcpu;

    //get vendorstring
    unsigned int eax,ebx,ecx,edx;

    //get vendor & maxleafs
    __get_cpuid(0, &eax, (uint32_t *) &acpu->cpuid_vendorstring[0], (uint32_t *) &acpu->cpuid_vendorstring[8], (uint32_t *) &acpu->cpuid_vendorstring[4]);
    acpu->cpuid_vendorstring[12]=0; // terminate
    acpu->cpuid_maxbasicleafs=eax;

    //get features
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    acpu->cpuid_feature_ebx = ebx;
    acpu->cpuid_feature_ecx = ecx;
    acpu->cpuid_feature_edx = edx;

    if (edx & CPUID_FEAT_EDX_APIC) {
        int xAPIXid = ebx>>24; //get_cpu_coreid();
        acpu->xapic_logicalid = xAPIXid;
        apic_id_cpumap[xAPIXid] = cpup;
    }
}

int arch_cpu_hasfeature_APIC(void) {
    unsigned int eax,ebx,ecx,edx;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    if (edx & CPUID_FEAT_EDX_APIC) {
        return 1;
    }
    return 0;
}

int arch_cpu_hasfeature_RDTSC(void) {
    unsigned int eax,ebx,ecx,edx;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    if (edx & CPUID_FEAT_EDX_TSC) {
        return 1;
    }
    return 0;
}