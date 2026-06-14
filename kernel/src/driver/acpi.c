#include "log.h"
#include "types.h"
#include "driver/acpi.h"
#include "mm.h"
#include "error.h"
#include <string.h>


RSDP_t* acpi_find_rsdp(void) {
    /*
    // 1. Skanna EBDA (läs basadressen från 0x040E)
    uint16_t ebda_segment = *(uint16_t *) P2V(0x040E);
    log_msg("ebda_segment = 0x%lx\n", ebda_segment);
    uint32_t ebda_phys = (uint32_t) ebda_segment << 4;

    if (ebda_phys >= 0x80000 && ebda_phys < 0xA0000) {
        for (adr_t addr = P2V(ebda_phys); addr < P2V(ebda_phys + 1024); addr += 16) {
            if (memcmp((void *) (uintptr_t) P2V(addr), "RSD PTR ", 8) == 0) {
                log_msg("found RSD PTR (from EBDA) at 0x%lx", addr);
                return (RSDP_t *) (uintptr_t) addr;
            }
        }
    }
    */
    // 2. Skanna BIOS-regionen (0xE0000 - 0xFFFFF)
    for (adr_t addr = P2V(0xE0000); addr < P2V(0xFFFFF); addr += 16) {
        if (memcmp((void *) (uintptr_t) addr, "RSD PTR ", 8) == 0) {
            log_msg("found RSD PTR (from BIOS 0xE0000-0xFFFFF) at 0x%lx\n", addr);
            return (RSDP_t *) (uintptr_t) addr;
        }
    }

    return NULL; // RSDP hittades inte
}

RSDT_t* acpi_find_rsdt(void) {
    RSDP_t* rsdp = acpi_find_rsdp();
    log_msg("got rsdp at 0x%lx\n", rsdp);
    adr_t sdtadr = rsdp->rsdtAddress;
    if (rsdp->revision > 1) {
        //USE XSDP instead
        PANIC("TODO, implement XSDP (RSDP rev2 64bit addr)");
    }
    RSDT_t* rsdt = (RSDT_t*) P2V(sdtadr);
    return rsdt;
}

void acpi_debugprint_entries_v1(RSDT_t* rsdt) {
    int entries = (rsdt->header.length - sizeof(SDT_t)) / 4;
    for (int i = 0; i < entries; i++) {
        SDT_t* sdt = (SDT_t*) P2V((uint64_t)rsdt->entries[i]);
        char sig[5];
        strlcpy(sig, sdt->signature, 5);
        log_msg("got sdt(%d) sig=%s \n", i,sig);
    }

}

void acpi_debugprint_entries(void) {
    RSDP_t* rsdp = acpi_find_rsdp();
    if (rsdp==NULL)
        return;

    log_msg("got rsdp at 0x%lx\n", rsdp);
    adr_t sdtadr = rsdp->rsdtAddress;
    if (rsdp->revision > 1) {
        //USE XSDP instead
        PANIC("TODO, implement XSDP (RSDP rev2 64bit addr)");
    }else {
        RSDT_t* rsdt = (RSDT_t*) P2V(sdtadr);
        int entries = (rsdt->header.length - sizeof(SDT_t)) / 4;
        for (int i = 0; i < entries; i++) {
            SDT_t* sdt = (SDT_t*) P2V((uint64_t)rsdt->entries[i]);
            char sig[5];
            strlcpy(sig, sdt->signature, 5);
            log_msg("found sig %s from SDT at 0x%lx\n", sig,sdt);
        }
    }
}

SDT_t* acpi_find(char* signature) {
    RSDP_t* rsdp = acpi_find_rsdp();
    if (rsdp==NULL)
        return NULL;

    log_msg("got rsdp at 0x%lx\n", rsdp);
    adr_t sdtadr = rsdp->rsdtAddress;
    if (rsdp->revision > 1) {
        //USE XSDP instead
        PANIC("TODO, implement XSDP (RSDP rev2 64bit addr)");
    }else {
        RSDT_t* rsdt = (RSDT_t*) P2V(sdtadr);
        int entries = (rsdt->header.length - sizeof(SDT_t)) / 4;
        for (int i = 0; i < entries; i++) {
            SDT_t* sdt = (SDT_t*) P2V((uint64_t)rsdt->entries[i]);
            if (memcmp(sdt->signature, signature, 4) == 0) {
                log_msg("found sig %s from SDT at 0x%lx\n", signature,sdt);
                return sdt;
            }
        }
    }
    return NULL;
}

/*
void acpi_test_sdt(void) {
    RSDT_t* rsdt = acpi_find_rsdt();
    log_msg("got rsdt at 0x%lx\n", rsdt);
    size_t length = rsdt->header.length;
    char sig[5];
    //memcpy(sig, rsdt->header.signature, 4);
    strlcpy(sig, rsdt->header.signature, 5);
    //sig[4]=0;
    log_msg("got rsdt sig=%s length=%d\n", sig,length);
    acpi_debugprint_entries_v1(rsdt);
    HPET_t* hpet = (HPET_t*) acpi_find("HPET");
    log_msg("got hpet at 0x%lx\n",hpet);
    return;
}
*/