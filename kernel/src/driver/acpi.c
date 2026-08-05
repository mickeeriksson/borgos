#include "log.h"
#include "types.h"
#include "driver/acpi.h"
#include "mm.h"
#include "error.h"
#include <string.h>
#include "bootmem.h"

acpi_info_t acpi_info;

RSDP_t* acpi_find_rsdp(void) {

    if (acpi_info.uefi_xsdp_adr!=0) {
        log_msg("Get RDSP from UEFI xsdp old (v2.0)\n");
        acpi_info.version = 2;
        return (RSDP_t*) P2V(acpi_info.uefi_xsdp_adr);
    }

    if (acpi_info.uefi_rsdp_adr!=0) {
        log_msg("Get RDSP from UEFI rsdp old (v1.0)\n");
        acpi_info.version = 1;
        return (RSDP_t*) P2V(acpi_info.uefi_rsdp_adr);
    }

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
            acpi_info.version = 0;
            return (RSDP_t *) (uintptr_t) addr;
        }
    }

    return NULL; // RSDP hittades inte
}

/*
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
*/

SDT_t* acpi_get_sdt(void* tbl,size_t entrysize,int idx) {
    if (entrysize==4) {
        uint32_t* entries = (uint32_t*) tbl;
        return (SDT_t*) P2V((adr_t) entries[idx]);
    }
    if (entrysize==8) {
        uint64_t* entries = (uint64_t*) tbl;
        return (SDT_t*) P2V((adr_t) entries[idx]);
    }
    PANIC("Unexpected entrysize");
    return 0;
}

void acpi_debugprint_entries(void) {

    //RSDP_t* rsdp = acpi_find_rsdp();
    //if (rsdp==NULL)
    //    return;
    //log_msg("got rsdp at 0x%lx\n", rsdp);
    adr_t sdtadr = acpi_info.sdtadr;
    if (sdtadr==0) {
        PANIC("SDT root not set");
    }

    SDT_t* roothdr = (SDT_t*) sdtadr;
    log_msg("FOUND SDT root HDR at %#lx\n", roothdr);


    for (int i=0 ; i<acpi_info.entrycount; i++) {
        SDT_t* sdt = acpi_get_sdt(acpi_info.entries,acpi_info.sdtptrsize,i);
        //log_msg("got sdt at %#lx\n", sdt);
        char sig[5];
        strlcpy(sig, sdt->signature, 5);
        log_msg("found sig %s from SDT at 0x%lx\n", sig,sdt);
    }

/*
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
    }*/
    return;
}


SDT_t* acpi_find(char* signature) {
    /*RSDP_t* rsdp = acpi_find_rsdp();
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
    }*/

    adr_t sdtadr = acpi_info.sdtadr;
    if (sdtadr==0) {
        PANIC("SDT root not set");
    }

    SDT_t* roothdr = (SDT_t*) sdtadr;
    log_msg("FOUND SDT root HDR at %#lx\n", roothdr);

    for (int i=0 ; i<acpi_info.entrycount; i++) {
        SDT_t* sdt = acpi_get_sdt(acpi_info.entries,acpi_info.sdtptrsize,i);
        //log_msg("got sdt at %#lx\n", sdt);
        if (memcmp(sdt->signature, signature, 4) == 0) {
            log_msg("found sig %s from SDT at 0x%lx\n", signature,sdt);
            return sdt;
        }
        //char sig[5];
        //strlcpy(sig, sdt->signature, 5);
        //log_msg("found sig %s from SDT at 0x%lx\n", sig,sdt);
    }

    return NULL;
}

void acpi_map_acpi_mem(adr_t adr_phys ) {
    //FIND AND MAP ACPI
    struct bootmem_memmapentry *ptr = bootmem_memmapentry;

    log_msg("MAP memory around Root SDT adr %#lx\n",adr_phys);
    while (ptr->type != BOOTMEMTYPE_END) {
        //log_msg("TEST ACPI DEVICE MAPPING FOR type = %d (0x%x), start=%#lx  size=%#lx\n", ptr->type,ptr->type,ptr->addr,ptr->size);
        if (adr_phys > ptr->addr && adr_phys < (ptr->addr + ptr->size-1)) {
            log_msg("  FOUND MEMORY AREA type = %d (0x%x), start=%#lx  size=%#lx\n", ptr->type,ptr->type,ptr->addr,ptr->size);
            if ((ptr->type == BOOTMEMTYPE_RESERVED) || (ptr->type == BOOTMEMTYPE_ACPI_RECLAIMABLE) || (ptr->type == BOOTMEMTYPE_NVS)) {
                adr_t acpistart = PAGEALIGN_DOWN(ptr->addr);
                adr_t acpiend = (ptr->addr + ptr->size-1);
                log_msg("  THIS IS ACPI AREA, MAP AS DEVICE 0x%lx - 0x%lx\n",acpistart,acpiend);
                mmu_map_device_region(acpistart, acpiend);
                return;
            }else {
                log_msg("  !!! THIS AREA IS MATCH, BUT IS NOT BOOTMEMTYPE_RESERVED ??? \n");
            }
        }
        ptr++;
    }
}


static int checksum_ok(const void *p, size_t len) {
    const uint8_t *b = p;
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + b[i]);
    }
    return sum == 0;
}

static int rsdp_valid(RSDP_t* r) {
    static const char sig[8] = { 'R','S','D',' ','P','T','R',' ' };
    for (int i = 0; i < 8; i++)
        if (r->signature[i] != sig[i])
            return 0;
    if (!checksum_ok(r, 20))
        return 0;                 /* v1-delen */
    if (r->revision >= 2) {
        XSDP_t* x;
        x = (XSDP_t*) r;
        if (x->length < sizeof(XSDP_t))
            return 0;
        if (x->length > 4096)
            return 0;
        if (!checksum_ok(x, x->length))
            return 0;      /* v2-delen */
    }
    return 1;
}

void acpi_memmap_acpi_entries(void) {
    adr_t sdtadr = acpi_info.sdtadr;
    if (sdtadr==0) {
        PANIC("SDT root not set");
    }
    SDT_t* roothdr = (SDT_t*) sdtadr;
    log_msg("FOUND SDT root HDR at %#lx\n", roothdr);

    for (int i=0 ; i<acpi_info.entrycount; i++) {
        SDT_t* sdt = acpi_get_sdt(acpi_info.entries,acpi_info.sdtptrsize,i);
        log_msg("map memory for SDT at 0x%lx\n", V2P(sdt));
        acpi_map_acpi_mem(V2P(sdt));
        //log_msg("got sdt at %#lx\n", sdt);
        char sig[5];
        strlcpy(sig, sdt->signature, 5);
        log_msg("found sig %s from SDT at 0x%lx\n", sig,sdt);
    }
}

void acpi_tables_init(void) {
    RSDP_t* rsdp = acpi_find_rsdp();
    if (rsdp==NULL) {
        PANIC("Unable to find RSDP");
    }

    if (acpi_info.version == 2) {
        if (!rsdp_valid(rsdp)) {
            if (acpi_info.uefi_rsdp_adr!=0) {
                log_msg("Revert XSDP to RSDP (not valid)\n");
                acpi_info.version = 1;   //revert to version 1;
                rsdp = (RSDP_t*) P2V(acpi_info.uefi_rsdp_adr);
            }
        }else {
            acpi_info.version=2;
            acpi_info.sdtptrsize=8;
            acpi_info.sdtadr = P2V( ((XSDP_t*)rsdp)->xsdtAddress );
        }
    }
    if (acpi_info.version == 1) {
        if (!rsdp_valid(rsdp)) {
            PANIC("RSDP not valid\n");
        }else {
            acpi_info.version=1;
            acpi_info.sdtptrsize=4;
            acpi_info.sdtadr = P2V( rsdp->rsdtAddress );
        }
    }
    if (acpi_info.version == 0) {
        if (!rsdp_valid(rsdp)) {
            PANIC("RSDP (from scan) not valid\n");
        }else {
            acpi_info.version = 1;
            acpi_info.sdtptrsize=4;
            acpi_info.sdtadr = P2V( rsdp->rsdtAddress );
        }
    }

    //map acpi memory before looking at RSDT or XSDT
    adr_t rsdtadr = V2P(acpi_info.sdtadr);
    acpi_map_acpi_mem(rsdtadr);


    if (acpi_info.version >= 2) {
        SDT_t* x = (SDT_t*) acpi_info.sdtadr;
        acpi_info.entrycount = (x->length - sizeof(SDT_t)) / 8;
        acpi_info.entries = (void*) ((adr_t) x) + sizeof(SDT_t);
    }else {
        SDT_t* r = (SDT_t*) acpi_info.sdtadr;
        acpi_info.entrycount = (r->length - sizeof(SDT_t)) / 4;
        acpi_info.entries = (void*) ((adr_t) r) + sizeof(SDT_t);
    }
    acpi_memmap_acpi_entries();
    log_msg("ACPI Tables init done\n");
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