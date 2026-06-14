#include "types.h"
#include "bootmem.h"
#include "error.h"
#include "mm.h"

//adr_t bootmem_start_address;
//adr_t bootmem_heaptop

struct bootmem_info bootmeminfo;

struct bootmem_memmapentry *bootmem_memmapentry;


void bootmem_debugprint(void) {
    log_msg("bootmem start   = 0x%x\n", bootmeminfo.addr);
    log_msg("bootmem top     = 0x%x\n", bootmeminfo.heaptop);
}

void bootmem_memmnap_debugprint(void) {
    struct bootmem_memmapentry *ptr = bootmem_memmapentry;
    log_msg("boot memmap\n", ptr->type);
    while (ptr->type != BOOTMEMTYPE_END) {
        log_msg("type = %d (0x%x)\n", ptr->type,ptr->type);
        log_msg("  addr = 0x%lx", ptr->addr);
        size_t amb = ptr->addr / (1024*1024);
        if (amb > 0) {
            log_msg("  (%d Mb)\n", amb);
        }else {
            log_msg("  (%d Kb)\n", ptr->addr / (1024));
        }



        log_msg("  size = 0x%lx, end = 0x%lx", ptr->size,(ptr->addr + ptr->size-1));
        size_t szmb = ptr->size / (1024*1024);
        if (szmb > 0) {
            log_msg("  (%d Mb)\n", szmb);
        }else {
            log_msg("  (%d Kb)\n", ptr->size / (1024));
        }


        int pfnstart = PHYS2PFN(ptr->addr);
        int pfnend = PHYS2PFN(ptr->addr + ptr->size-1);
        log_msg("  pfn = %d - %d (%d)\n", pfnstart,pfnend,(pfnend-pfnstart)+1);

        ptr++;
    }
}


void* bootmem_alloc(size_t size)
{
    //if(bootmem_valid == 0){
    //    PANIC("CANT ALLOC WHEN Bootmem has been disabled");
    //}
    void* ptr = (void*) bootmeminfo.heaptop;
    bootmeminfo.heaptop+=size;
    log_msg("bootmem_alloc %#x, Next free adress is now %#x\n",ptr,bootmeminfo.heaptop);
    return (void*) P2V(ptr);
}

void* bootmem_alloc_phys(size_t size,size_t align)
{
    adr_t mask = ~(align-1);
    adr_t ptr = (bootmeminfo.heaptop + align -1) & mask;

    bootmeminfo.heaptop= ptr + size;
    log_msg("bootmem_alloc %#x, Next free adress is now %#x\n",ptr,bootmeminfo.heaptop);
    return (void*) ptr;
}

void* bootmem_alloc_virt(size_t size,size_t align)
{
    adr_t mask = ~(align-1);
    adr_t ptr = (bootmeminfo.heaptop + align -1) & mask;

    bootmeminfo.heaptop= ptr + size;
    log_msg("bootmem_alloc %#x, Next free adress is now %#x\n",ptr,bootmeminfo.heaptop);
    return (void*) P2V(ptr);
}