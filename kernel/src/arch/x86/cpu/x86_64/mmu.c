#include "log.h"
#include "error.h"
#include "bootmem.h"
#include "mm.h"
#include "cpu/mmu.h"

extern void* pagetable_init(void);
extern pgtblrecord_t* pagetable_walk_getlvl1table(pgtblrecord_t* lvl4table, adr_t vaddr,uint64_t tblcreateflags);
extern void pagetable_debugwalk(pgtblrecord_t* lvl4table, adr_t vaddr) ;

void* kernelpagetable;


#define MMU_PTE_US   (1<<2)  //U/S bit,
#define MMU_PTE_PWT  (1<<3)  //PCD bit,
#define MMU_PTE_PCD  (1<<4)  //PCD bit,



void mmu_asm_set_cr3(uint64_t cr3_val) {
    log_msg("Setting CR3 to 0x%lx\n", cr3_val);
    __asm__ __volatile__(
        "mov %0, %%cr3"
        : /* Inga utmatningar */
        : "r" (cr3_val) /* Inmatning: cr3_val placeras i ett generellt register */
        : "memory"      /* Berättar för kompilatorn att minnesordningen kan ändras, eftersom TLB flushas */
    );
}

uint64_t mmu_asm_get_cr3(void) {
    uint64_t cr3_val;
    __asm__ __volatile__(
        "mov %%cr3, %0"
        : "=r" (cr3_val) // Utmatning: placera värdet i cr3_val
        :                // Inga inmatningar
    );
    return cr3_val;
}

static inline void _mmu_pte_setbase(pagetableentry_t* entry,adr_t base)
{
    entry->base = (base >> 12) & 0x000FFFFFFFFF; // 36 bit adress
    //log_msg("Base is now 0x%lx\n",entry->base);
}

static inline void _mmu_pte_setrawflags(pagetableentry_t* entry,uint64_t flags)
{
    entry->rawflags = flags;
}

//--- Lvl 4 pagetable at 0xFFFF800000FDC000
//--- Lvl 3 pagetable at 0xFFFF800000FDD000
//--- Lvl 2 pagetable at 0xFFFF800000FD0000

//--- Lvl 4 pagetable at 0xFFFF800000FD1000
//--- Lvl 3 pagetable at 0xFFFF800000FD2000
//--- Lvl 2 pagetable at 0xFFFF800000FD3000


void mmu_map_page(void* pagetable,adr_t paddr,adr_t vaddr,uint64_t flags) {
    //TODO
    uint64_t pteflags = 0x23; //PRESENT, WRITABLE, ACCESSED
    uint64_t dircreateflags = 0x23; //PRESENT, WRITABLE, ACCESSED

    if (flags & MMU_DEVICE) {
        pteflags |= MMU_PTE_PCD | MMU_PTE_PWT; //set PCD bit, disable caching
    }
    if (flags & MMU_USER) {
        pteflags |= MMU_PTE_US;
        dircreateflags |= MMU_PTE_US;
    }



    //log_msg("\n Map PAGE addr 0x%lx\n", vaddr);
    uint64_t lvl4idx = (vaddr >> (12+9+9+9)) & 0x1FF;
    if (lvl4idx>256) {
        //TODO if mapping of > 256 is allowed, theese pages must also be mapped at boot,
        //otherwise they will not be copied to user  pgtbl later.
        PANIC("Mapping of kernelpages > lvl4idx of 256 is not allowed\n");
    }
    if (lvl4idx>0 && lvl4idx<256) {
        //TODO if mapping of > 256 is allowed, theese pages must also be mapped at boot,
        //otherwise they will not be copied to user  pgtbl later.
        PANIC("Mapping of userpages > 512Gb is not allowed\n");
    }

    pgtblrecord_t* lvl1tbl = pagetable_walk_getlvl1table(pagetable, vaddr,dircreateflags);
    //log_msg("Lvl1 (leaf) Page table at 0x%lx\n", lvl1tbl);
    uint64_t lvl1idx = (vaddr >> 12) & 0x1FF;
    pagetableentry_t* entry = (pagetableentry_t*) &lvl1tbl[lvl1idx];
    _mmu_pte_setbase(entry,paddr);
    _mmu_pte_setrawflags(entry,pteflags);
    //log_msg("Entry at 0x%lx after map_page = 0x%lx\n", entry, *((uint64_t*)entry));
    //PANIC("TODO");
}

void mmu_map_kernelpage(adr_t paddr,adr_t vaddr,uint64_t flags) {
    mmu_map_page(kernelpagetable,paddr, vaddr, flags) ;
}

void mmu_map_range(void* pagetable,adr_t paddr,adr_t vaddr,size_t bytes,uint64_t flags) {
    adr_t vaddr_start = vaddr;
    adr_t vaddr_end = vaddr_start + bytes;

    while (vaddr < vaddr_end) {
        mmu_map_page(pagetable,paddr,vaddr,flags) ;
        log_msg("   MAP User page paddr=0x%lx  user vaddr=0x%lx\n",paddr,vaddr);
        vaddr += PAGESIZE;
        paddr += PAGESIZE;
    }
}

void mmu_debug_walkadr(void* pagetable,adr_t vaddr) {
    log_msg("Debug WALK %#x\n",vaddr);
    pagetable_debugwalk(pagetable, vaddr);
}


void mmu_init_kernel_pagetable_mem(void) {
    //init low memory < 4gb.
    struct bootmem_memmapentry *ptr = bootmem_memmapentry;
    //log_msg("boot memmap\n", ptr->type);
    while (ptr->type != BOOTMEMTYPE_END) {
        if (ptr->type == BOOTMEMTYPE_FREE) {
            log_msg("\n\nPAGE MAP NORMAL MEMORY\n");
            log_msg("type = %d (0x%x)\n", ptr->type,ptr->type);
            log_msg("  addr=0x%lx", ptr->addr);
            int pfnstart = PHYS2PFN(ptr->addr);
            int pfnend = PHYS2PFN(ptr->addr + ptr->size-1);
            log_msg("  pfn=%d-%d", pfnstart,pfnend);
            log_msg("  size=%dKb\n", (ptr->size / 1024));

            //adr_t startadr = PAGEALIGN_DOWN(ptr->addr);
            //adr_t endadr = PAGEALIGN_DOWN(ptr->addr);

            if (ptr->addr != PAGEALIGN_DOWN(ptr->addr)) {
                PANIC("This Area does not start on exact page\n");
            }
            for (int pfn = pfnstart; pfn <= pfnend; pfn++) {
                //TODO fix flags to represent normal memory....
                //if (pfn==256) {
                //    log_msg("------break\n");
                //}
                //if (pfn==159) {
                //    log_msg("------break\n");
                //}
                //log_msg("MAP pfn = %d, identity paddr=0x%lx  vaddr=0x%lx\n",pfn,PFN2PHYS(pfn),PFN2PHYS(pfn));
                mmu_map_page( kernelpagetable,PFN2PHYS(pfn),PFN2PHYS(pfn),0); // identitymapping
                //log_msg("MAP pfn = %d, kernel paddr=0x%lx  vaddr=0x%lx\n",pfn,PFN2PHYS(pfn),PFN2VIRT(pfn));
                mmu_map_page( kernelpagetable,PFN2PHYS(pfn),PFN2VIRT(pfn),0); //
                //log_msg("  MAP page pfn=%d\n", pfn);
            }

        }

        ptr++;
    }
}

void mmu_map_device_region(adr_t adrstart,adr_t adrend) {
    int pfnstart = PHYS2PFN(adrstart);
    int pfnend = PHYS2PFN(adrend);
    for (int pfn = pfnstart; pfn <= pfnend; pfn++) {
        //log_msg("MAP pfn = %d, identity paddr=0x%lx  vaddr=0x%lx\n",pfn,PFN2PHYS(pfn),PFN2PHYS(pfn));
        //mmu_map_page( kernelpagetable,PFN2PHYS(pfn),PFN2PHYS(pfn),0); // identitymapping
        if (pfn==pfnstart || pfn==pfnend) {
            log_msg("MAP Device pfn = %d, kernel paddr=0x%lx  vaddr=0x%lx (log only first & last pfn)\n",pfn,PFN2PHYS(pfn),PFN2VIRT(pfn));
        }
        //log_msg("MAP Device pfn = %d, kernel paddr=0x%lx  vaddr=0x%lx\n",pfn,PFN2PHYS(pfn),PFN2VIRT(pfn));
        mmu_map_page( kernelpagetable,PFN2PHYS(pfn),PFN2VIRT(pfn),MMU_DEVICE); //
    }
}

/*
void mmu_init_kernel_pagetable_devices(void) {
    //init typical x64_devices.
    //PANIC("TODO");
}*/

void mmu_init_kernel_pagetable(void) {
    log_msg("INIT Kernel PageTable, to replace pagetable from boot\n");
    kernelpagetable = pagetable_init();
    log_msg("Kernel pagetable at 0x%lx\n", kernelpagetable);

    mmu_init_kernel_pagetable_mem();
    //mmu_init_kernel_pagetable_devices();


    //switch table by setting cr3
    mmu_asm_set_cr3(V2P(kernelpagetable));
    //PANIC("TODO");
}

void mmu_init_user_pagetable(adr_t kpgdir,adr_t upgdir) {
    log_msg("INIT User PageTable from Kernel pagetable\n");
    uint64_t* kptbl4 = (uint64_t*) kpgdir;
    uint64_t* uptbl4 = (uint64_t*) upgdir;
    // copy only upper half
    for (int i=256;i<512;i++) {
        uint64_t kentryraw = kptbl4[i];
        //pagetableentry_t* kentry = (pagetableentry_t*) &kptbl4[i];
        if (kentryraw > 0) {
            //adr_t phys = i << (12+9+9);
            log_msg("Copy entry %d = %#lx \n", i,kentryraw);
            uptbl4[i] = kptbl4[i];
        }
    }

}