#include "cpu/gdt.h"
#include "log.h"
#include "cpu.h"
#include "error.h"
#include "mm.h"
#include <string.h>

gdt_entry_64_t bootgdt[GDTSIZE];    //boot gdt
gdtr_ptr_t gdtr;                //boot gdt ptr

void* per_cpu_gdtpage;          //per cpu gdt


gdtr_ptr_t gdt_sgdt (void) {
    gdtr_ptr_t gdtr;
    __asm__ volatile ("sgdt %0" : "=m" (gdtr));

    log_msg("GDT Basadress: 0x%lx\n", (unsigned long long)gdtr.base);
    log_msg("GDT Size:     0x%x\n", gdtr.size);

    return gdtr;
}

void gdt_lgdt (adr_t gdtptr_virtaddr) {
    __asm__ volatile("lgdt (%0)" : : "r" (gdtptr_virtaddr));
}

void gdt_ltr(uint16_t selector)
{
    __asm__ volatile("ltr %w0" :: "r"(selector) : "memory");
}

void clang_flush_gdt64_ext(adr_t gdt_ptr_addr, uint64_t code_sel, uint64_t data_sel) {
    __asm__ volatile(
        // 1. Ladda GDT-pekaren in i GDTR-registret
        "lgdt (%0)\n"

        // 2. Uppdatera alla datasegmentregister med data_sel
        "mov %2, %%rax\n"
        "mov %%rax, %%ds\n"
        "mov %%rax, %%es\n"
        "mov %%rax, %%fs\n"
        "mov %%rax, %%gs\n"
        "mov %%rax, %%ss\n"

        // 3. Utför en Far Return (lretq) för att ladda om CS och RIP
        "pushq %1\n"               // Tryck ned den valda kodselektorn (CS)
        "leaq 1f(%%rip), %%rax\n"  // Hämta RIP-relativ adress för etiketten '1'
        "pushq %%rax\n"            // Tryck ned måladressen (RIP) på stacken
        "lretq\n"                  // Poppar RIP och CS samtidigt

        "1:\n"                     // Hit hoppar lretq
        :
        : "r" (gdt_ptr_addr),        // %0
          "r" (code_sel),            // %1
          "r" (data_sel)             // %2
        : "rax", "cc", "memory"      // Berätta för Clang vilka register/resurser vi ändrar
    );
}

void gdt_debug_GDT_entry_64(gdt_entry_64_t* entry) {
    int access = entry->type | entry->s << 4 | entry->dpl << 5 | entry->p << 7;
    log_msg("    Access: 0x%x\n", access);
    log_msg("    Access (type): 0x%0x\n", entry->type);
    log_msg("    Access (s)   : 0x%x\n", entry->s);
    log_msg("    Access (dpl) : 0x%x\n", entry->dpl);
    log_msg("    Access (p)   : 0x%x\n", entry->p);
}


void gdt_debugCurrentGDT(void){
    gdtr_ptr_t gdtr;
    gdtr = gdt_sgdt ();

    int entrysize = sizeof(gdt_entry_64_t);
    int tblsize = (gdtr.size+1)/entrysize;
    gdt_entry_64_t* gdttable = (gdt_entry_64_t*) gdtr.base;


    gdt_entry_64_t* entry;
    for (int i = 0; i < tblsize; i++) {
        entry = &(gdttable[i]);
        uint64_t entry64raw = *((uint64_t*) &(gdttable[i]));

        log_msg("GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n", i,tblsize,entry,entry64raw);
        gdt_debug_GDT_entry_64(entry);
    }
    /*
    let gp : GDTPointer = asm_sgdt();

    let entries = (gp.limit+1)/8;
    for i in 0..entries{
        let eadr = gp.base + (i*8) as u64;
        let ptr = eadr as *const u64;
        let entry = unsafe{ &*ptr};
        debug!("GDT {} = {:#X} ({})",i,entry,entry);
        let access = (entry >> 40) & 0xFF;
        let flags = (entry >> 52) & 0x0F;
        debug!("  > flags={:#X} access={:#X} ",flags,access);

    }
    */
    log_msg("\n");
    //let _x=3;
}

void gdt_flush_gdt64_current_cpu() {
    cpu_t* cpup = CURRENTCPU;
    clang_flush_gdt64_ext((adr_t) &cpup->archcpu.gdtptr, GDT64_KERNEL_CS ,GDT64_KERNEL_DS);
    log_msg("\nLoaded new GDT Table for CPU:%d\n",cpup->cpuid);
    //gdt_debugCurrentGDT();
}

void gdt_install_tss_current_cpu() {
    cpu_t* cpup = CURRENTCPU;

    gdt_entry_64_t* gdtp = cpup->archcpu.per_cpu_gdt;
    void *gdt_base = gdtp;
    tss64_t *tss = &cpup->archcpu.tss;
    log_msg("TSS virt addr: 0x%lx \n", tss);
    //Allocate Stack for IST1
    void* ist1virt = page_alloc_pages_virt(0, 0); // 4kb
    tss->ist[0] = (uint64_t) ist1virt+PAGESIZE-64;   // ist[0] är samma sak som ist1. Då arrayen pekar ut ist1-ist7
    log_msg("IST1 ist1virt addr: 0x%lx  size:0x%lx\n", ist1virt,PAGESIZE);

    void* ist2virt = page_alloc_pages_virt(0, 0); // 4kb
    tss->ist[1] = (uint64_t) ist2virt+PAGESIZE-64;   //ist2
    log_msg("IST2 ist2virt addr: 0x%lx  size:0x%lx\n", ist2virt,PAGESIZE);

    void* ist3virt = page_alloc_pages_virt(0, 0); // 4kb
    tss->ist[2] = (uint64_t) ist3virt+PAGESIZE-64;  //ist3
    log_msg("IST3 ist3virt addr: 0x%lx  size:0x%lx\n", ist3virt,PAGESIZE);


    uint64_t base  = (uint64_t)tss;
    uint32_t limit = sizeof(tss64_t) - 1;

    tss_descriptor_t d = {0};
    d.lim_15_0   = limit & 0xFFFF;
    d.lim_19_16  = (limit >> 16) & 0xF;
    d.base_15_0  = base & 0xFFFF;
    d.base_23_16 = (base >> 16) & 0xFF;
    d.base_31_24 = (base >> 24) & 0xFF;
    d.base_63_32 = (uint32_t)(base >> 32);
    d.type = 0x9;
    d.s    = 0;
    d.dpl  = 0;
    d.p    = 1;

    memcpy((uint8_t *)gdt_base + GDT64_TSS, &d, sizeof(d));

    tss->iopb_offset = sizeof(tss64_t);   // ingen I/O-bitmap
    //asm volatile("ltr %w0" :: "r"((uint16_t)GDT64_TSS));
    gdt_ltr(GDT64_TSS);
    return;
}


void gdt_setup_gdt(gdt_entry_64_t *gdt) {


    for (int i = 0; i < GDTSIZE; i++) {
        uint64_t* entry64raw = ((uint64_t*) &(gdt[i]));
        *entry64raw = 0;
        entry64raw++;
        *entry64raw = 0;

        //gdt_entry_64_t *entry = &gdt[i];
        //log_msg("Nulling GDT Entry: %d/%d at 0x%lx  entry : 0x%lx\n", i,GDTSIZE,entry,*entry);
    }
    //Entry 0 is null descriptor

    //Entry 1 is KERNEL CODE long descriptor
    gdt_entry_64_t* kcode64 = &gdt[1];
    kcode64->lim_15_0 = 0xFFFF;
    kcode64->base_15_0 = 0x0000;
    kcode64->base_23_16 = 0x00;
    kcode64->type = (1<<1) | (1<<3) | (1<<0); //rw(1) + execute(3) + accesed(0)
    kcode64->s = 1; //1=Code or Data segment
    kcode64->dpl = 0; //Kernel (highest level)
    kcode64->p = 1; //Is Present
    kcode64->lim_19_16 = 0xF;
    kcode64->avl = 0; //unsused
    kcode64->l = 1; //Long Mode
    kcode64->db = 0; //not set for long mode code segment even if it is 32bit+
    kcode64->g = 1; //Granularity 1= limmit is in 4k granularity
    kcode64->base_31_24 = 0x00;
    //kcode64->base_32_63 = 0x00000000;
    //kcode64->reserved_31_0 = 0x00000000;
    log_msg("SET GDT Entry: %d/%d (KERNEL 64 bit CODE) at 0x%lx  entry : 0x%lx\n",1,GDTSIZE,kcode64,*kcode64);


    //Entry 2 is KERNEL DATA long descriptor
    gdt_entry_64_t* kdata64 = &gdt[2];
    kdata64->lim_15_0 = 0xFFFF;
    kdata64->base_15_0 = 0x0000;
    kdata64->base_23_16 = 0x00;
    kdata64->type = (1<<1) | (1<<0); //rw(1) + accesed(0)
    kdata64->s = 1; //1=Code or Data segment
    kdata64->dpl = 0; //Kernel (highest level)
    kdata64->p = 1; //Is Present
    kdata64->lim_19_16 = 0xF;
    kdata64->avl = 0; //unsused
    kdata64->l = 0; //Long Mode (not set for 64 data segment)
    kdata64->db = 1; //32bit+ segment
    kdata64->g = 1; //Granularity 1= limmit is in 4k granularity
    kdata64->base_31_24 = 0x00;
    //kdata64->base_32_63 = 0x00000000;
    //kdata64->reserved_31_0 = 0x00000000;
    log_msg("SET GDT Entry: %d/%d (KERNEL 64 bit DATA) at 0x%lx  entry : 0x%lx\n",2,GDTSIZE,kdata64,*kdata64);

    //Entry 3 is USER DATA long descriptor
    gdt_entry_64_t* udata64 = &gdt[3];
    udata64->lim_15_0 = 0xFFFF;
    udata64->base_15_0 = 0x0000;
    udata64->base_23_16 = 0x00;
    udata64->type = (1<<1) | (1<<0); //rw(1) + accesed(0)
    udata64->s = 1; //1=Code or Data segment
    udata64->dpl = 3; //USER (highest level)
    udata64->p = 1; //Is Present
    udata64->lim_19_16 = 0xF;
    udata64->avl = 0; //unsused
    udata64->l = 0; //Long Mode (not set for 64 data segment)
    udata64->db = 1; //32bit+ segment
    udata64->g = 1; //Granularity 1= limmit is in 4k granularity
    udata64->base_31_24 = 0x00;
    //udata64->base_32_63 = 0x00000000;
    //udata64->reserved_31_0 = 0x00000000;
    log_msg("SET GDT Entry: %d/%d (USER 64 bit DATA)  at 0x%lx  entry : 0x%lx\n",3,GDTSIZE,udata64,*udata64);

    //Entry 4 is USER CODE long descriptor
    gdt_entry_64_t* ucode64 = &gdt[4];
    ucode64->lim_15_0 = 0xFFFF;
    ucode64->base_15_0 = 0x0000;
    ucode64->base_23_16 = 0x00;
    ucode64->type = (1<<1) | (1<<3) | (1<<0); //rw(1) + execute(3) + accesed(0)
    ucode64->s = 1; //1=Code or Data segment
    ucode64->dpl = 3; //USER (lowest level)
    ucode64->p = 1; //Is Present
    ucode64->lim_19_16 = 0xF;
    ucode64->avl = 0; //unsused
    ucode64->l = 1; //Long Mode
    ucode64->db = 0; //not set for long mode code segment even if it is 32bit+
    ucode64->g = 1; //Granularity 1= limmit is in 4k granularity
    ucode64->base_31_24 = 0x00;
    //ucode64->base_32_63 = 0x00000000;
    //ucode64->reserved_31_0 = 0x00000000;
    log_msg("SET GDT Entry: %d/%d (USER 64 bit CODE)  at 0x%lx  entry : 0x%lx\n",4,GDTSIZE,ucode64,*ucode64);



}

void gdt_init_per_cpu(void) {
    log_msg("gdt_init_per_cpu\n");
    size_t sizepercpu = GDTSIZE*sizeof(gdt_entry_64_t);
    log_msg("  size of  gdt_entry_64_t = %d\n", sizeof(gdt_entry_64_t));
    log_msg("  size per cpu = %d\n", sizepercpu);
    log_msg("  count per page = %d\n", PAGESIZE / sizepercpu);
    log_msg("  rest = %d\n", PAGESIZE % sizepercpu);

    if (GDTSIZE*16*16 != PAGESIZE) {
        PANIC("Current allocation is based on 16cpu gdt's per PAGE");
    }

    size_t gdtsize = cpu_numcores*sizepercpu;
    log_msg("  need size for all gdt's = %d\n", gdtsize);
    size_t porder = size_to_pageorder(gdtsize);
    log_msg("  porder = %d\n", porder);

    per_cpu_gdtpage = page_alloc_pages_virt(ZONENORMAL, porder);

    // build per cpu GDT
    for (int i = 0; i < cpu_numcores; i++) {
        cpu_t* cpup = &cpu[i];
        log_msg("Build GDT for CPU %d\n",cpup->cpuid);
        size_t offset = sizepercpu*i;
        gdt_entry_64_t* gdtp = per_cpu_gdtpage+offset;
        log_msg(" gdt_entry_64_t* = %#lx \n",gdtp);
        cpup->archcpu.per_cpu_gdt = gdtp;

        gdt_setup_gdt(gdtp);
        cpup->archcpu.gdtptr.size = GDTSIZE*sizeof(gdt_entry_64_t)-1;
        cpup->archcpu.gdtptr.base = (uint64_t) gdtp;
        log_msg("  \n",gdtp);
    }
    return;
}

void gdt_init(void) {
    log_msg("Init GDT\n");
    log_msg("previous GDT Table:\n");

    gdt_debugCurrentGDT();
    gdt_setup_gdt(&bootgdt[0]);
    gdtr.size = GDTSIZE*sizeof(gdt_entry_64_t)-1;
    gdtr.base = (uint64_t) &bootgdt[0];

    gdtr_ptr_t *dbggdtr = &gdtr;
    log_msg("gdt_ptr: 0x%lx\n", dbggdtr);
    clang_flush_gdt64_ext((adr_t) &gdtr, GDT64_KERNEL_CS ,GDT64_KERNEL_DS);

    log_msg("\nNEW GDT Table:\n");
    gdt_debugCurrentGDT();
}
